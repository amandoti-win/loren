// A tiny Cloudflare Worker + R2 upload host for Loren.
//
//   PUT /upload?name=shot.png&expires=86400   (Authorization: Bearer <UPLOAD_TOKEN>)
//       ->  {"key": "AbC123xY.png", "delete_token": "...", "expires_at": 1790000000}
//       "expires" is optional: seconds until the file is deleted (60 to one year).
//   GET /f/<key>                              ->  the file (public), 404 once expired or deleted
//   DELETE /f/<key>?token=<delete_token>      ->  {"deleted": true}
//
// An hourly cron trigger (see wrangler.jsonc) deletes expired files. Expired files also stop
// being served the moment they expire, even before the cron job removes them.

const INLINE = new Set(["image/png", "image/jpeg", "image/gif", "image/webp", "video/mp4", "video/webm", "text/plain"]);
const TYPES = { png: "image/png", jpg: "image/jpeg", jpeg: "image/jpeg", gif: "image/gif", webp: "image/webp", mp4: "video/mp4", webm: "video/webm", txt: "text/plain" };
const ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
const KEY = /^[A-Za-z0-9]{8}(\.[a-z0-9]{1,8})?$/;

const text = (status, body) => new Response(body + "\n", { status, headers: { "content-type": "text/plain" } });

async function sha256Hex(value) {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value));
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, "0")).join("");
}

async function sameSecret(given, expected) {
  const enc = new TextEncoder();
  const [a, b] = await Promise.all([given, expected].map((s) => crypto.subtle.digest("SHA-256", enc.encode(s))));
  return crypto.subtle.timingSafeEqual(a, b);
}

function newKey(ext) {
  const bytes = crypto.getRandomValues(new Uint8Array(8));
  return Array.from(bytes, (n) => ALPHABET[n % ALPHABET.length]).join("") + (ext ? "." + ext : "");
}

function newToken() {
  const bytes = crypto.getRandomValues(new Uint8Array(16));
  return Array.from(bytes, (n) => ALPHABET[n % ALPHABET.length]).join("");
}

const isExpired = (meta) => Number(meta?.expiresAt || 0) > 0 && Number(meta.expiresAt) <= Date.now() / 1000;

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);

    if (request.method === "PUT" && url.pathname === "/upload") {
      const given = (request.headers.get("authorization") || "").replace(/^Bearer /, "");
      if (!env.UPLOAD_TOKEN || !(await sameSecret(given, env.UPLOAD_TOKEN))) return text(401, "no");
      const length = Number(request.headers.get("content-length"));
      const max = Number(env.MAX_MB || 90) * 1024 * 1024;
      if (!(length > 0 && length <= max)) return text(413, "missing or too large");
      const name = url.searchParams.get("name") || "";
      const ext = name.includes(".") ? name.split(".").pop().toLowerCase().replace(/[^a-z0-9]/g, "").slice(0, 8) : "";

      const expires = url.searchParams.get("expires") || "";
      let expiresAt = 0;
      if (expires) {
        if (!/^\d+$/.test(expires) || Number(expires) < 60 || Number(expires) > 31536000) {
          return text(400, "expires must be 60 to 31536000 seconds");
        }
        expiresAt = Math.floor(Date.now() / 1000) + Number(expires);
      }

      const key = newKey(ext);
      const deleteToken = newToken();
      await env.FILES.put(key, request.body, {
        httpMetadata: { contentType: TYPES[ext] || "application/octet-stream" },
        customMetadata: { expiresAt: String(expiresAt), deleteHash: await sha256Hex(deleteToken) },
      });
      return Response.json({ key, delete_token: deleteToken, expires_at: expiresAt || null });
    }

    if ((request.method === "GET" || request.method === "HEAD") && url.pathname.startsWith("/f/")) {
      const key = url.pathname.slice(3);
      const object = KEY.test(key) ? await env.FILES.get(key) : null;
      if (!object) return text(404, "not found");
      if (isExpired(object.customMetadata)) {
        ctx.waitUntil(env.FILES.delete(key));
        return text(404, "not found");
      }
      const type = object.httpMetadata?.contentType || "application/octet-stream";
      const headers = {
        "content-type": type,
        "x-content-type-options": "nosniff",
        // Files that can expire must not be cached for a year.
        "cache-control": Number(object.customMetadata?.expiresAt || 0) > 0 ? "public, max-age=300" : "public, max-age=31536000, immutable",
      };
      if (!INLINE.has(type)) headers["content-disposition"] = "attachment";
      return new Response(request.method === "HEAD" ? null : object.body, { headers });
    }

    if (request.method === "DELETE" && url.pathname.startsWith("/f/")) {
      const key = url.pathname.slice(3);
      const object = KEY.test(key) ? await env.FILES.head(key) : null;
      if (!object) return text(404, "not found");
      const token = url.searchParams.get("token") || "";
      const hash = object.customMetadata?.deleteHash || "";
      if (!token || !hash || !(await sameSecret(await sha256Hex(token), hash))) return text(403, "wrong delete token");
      await env.FILES.delete(key);
      return Response.json({ deleted: true });
    }

    return text(404, "not found");
  },

  // Runs hourly: delete every file whose expiry time has passed.
  async scheduled(event, env, ctx) {
    let cursor;
    do {
      const page = await env.FILES.list({ cursor, include: ["customMetadata"], limit: 500 });
      const dead = page.objects.filter((o) => isExpired(o.customMetadata)).map((o) => o.key);
      if (dead.length) await env.FILES.delete(dead);
      cursor = page.truncated ? page.cursor : undefined;
    } while (cursor);
  },
};
