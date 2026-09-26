#!/usr/bin/env python3
"""A tiny upload server to pair with Lorgn. Stdlib only.

    UPLOAD_TOKEN=some-long-random-string ./server.py

PUT /upload?name=shot.png&expires=86400  (Authorization: Bearer <token>)
    ->  {"key": "AbC123xY.png", "delete_token": "...", "expires_at": 1790000000}
    "expires" is optional: seconds until the file is deleted (60 to one year). Without it the file is kept.
GET /f/<key>                                ->  the file (public), 404 once expired or deleted
DELETE /f/<key>?token=<delete_token>        ->  {"deleted": true}

It listens on 127.0.0.1 only: put a reverse proxy that does HTTPS (Caddy, nginx)
in front of it. Files are stored in ./uploads (override with UPLOAD_DIR).
"""
import hashlib
import hmac
import http.server
import json
import mimetypes
import os
import re
import secrets
import sys
import time
from pathlib import Path
from urllib.parse import parse_qs, urlparse

TOKEN = os.environ.get("UPLOAD_TOKEN", "")
ROOT = Path(os.environ.get("UPLOAD_DIR", "uploads")).resolve()
HOST = os.environ.get("HOST", "127.0.0.1")
PORT = int(os.environ.get("PORT", "8080"))
MAX_BYTES = int(os.environ.get("MAX_MB", "50")) * 1024 * 1024
KEY = re.compile(r"^[A-Za-z0-9]{8}(\.[a-z0-9]{1,8})?$")
META = ROOT / ".meta"
INLINE = ("image/png", "image/jpeg", "image/gif", "image/webp", "video/mp4", "video/webm", "text/plain")


def meta_path(key):
    return META / (key + ".json")


def read_meta(key):
    try:
        return json.loads(meta_path(key).read_text())
    except (OSError, ValueError):
        return {}


def remove(key):
    for path in (ROOT / key, meta_path(key)):
        try:
            path.unlink()
        except OSError:
            pass


def expired(key):
    at = read_meta(key).get("expires_at")
    return bool(at) and at <= time.time()


def sweep():
    """Delete every expired file. Runs on start-up and after each upload."""
    if not META.is_dir():
        return
    for path in META.glob("*.json"):
        key = path.name[: -len(".json")]
        if expired(key):
            remove(key)


class Handler(http.server.BaseHTTPRequestHandler):
    def send(self, status, body=b"", ctype="text/plain", extra=None):
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Content-Type-Options", "nosniff")
        for key, value in (extra or {}).items():
            self.send_header(key, value)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def do_PUT(self):
        url = urlparse(self.path)
        given = self.headers.get("Authorization", "").removeprefix("Bearer ")
        if url.path != "/upload" or not TOKEN or not hmac.compare_digest(given, TOKEN):
            return self.send(401 if url.path == "/upload" else 404, b"no\n")
        length = int(self.headers.get("Content-Length", -1))
        if not 0 < length <= MAX_BYTES:
            return self.send(413, b"missing or too large\n")
        name = parse_qs(url.query).get("name", [""])[0]
        ext = re.sub(r"[^a-z0-9]", "", Path(name).suffix.lower())[:8]
        key = secrets.token_urlsafe(6).replace("-", "a").replace("_", "b")[:8] + (f".{ext}" if ext else "")
        expires = parse_qs(url.query).get("expires", [""])[0]
        expires_at = None
        if expires:
            if not expires.isdigit() or not 60 <= int(expires) <= 31536000:
                return self.send(400, b"expires must be 60 to 31536000 seconds\n")
            expires_at = int(time.time()) + int(expires)
        delete_token = secrets.token_urlsafe(16)
        ROOT.mkdir(parents=True, exist_ok=True)
        META.mkdir(parents=True, exist_ok=True)
        with open(ROOT / key, "wb") as out:
            remaining = length
            while remaining:
                chunk = self.rfile.read(min(65536, remaining))
                if not chunk:
                    break
                out.write(chunk)
                remaining -= len(chunk)
        meta_path(key).write_text(json.dumps({
            "expires_at": expires_at,
            "delete_hash": hashlib.sha256(delete_token.encode()).hexdigest(),
        }))
        sweep()
        body = {"key": key, "delete_token": delete_token, "expires_at": expires_at}
        self.send(200, json.dumps(body).encode(), "application/json")

    def do_GET(self):
        key = self.path.removeprefix("/f/") if self.path.startswith("/f/") else ""
        path = ROOT / key
        if KEY.match(key) and expired(key):
            remove(key)
        if not KEY.match(key) or not path.is_file():
            return self.send(404, b"not found\n")
        ctype = mimetypes.guess_type(key)[0] or "application/octet-stream"
        extra = {} if ctype in INLINE else {"Content-Disposition": "attachment"}
        self.send(200, path.read_bytes(), ctype, extra)

    def do_DELETE(self):
        url = urlparse(self.path)
        key = url.path.removeprefix("/f/") if url.path.startswith("/f/") else ""
        if not KEY.match(key) or not (ROOT / key).is_file():
            return self.send(404, b"not found\n")
        token = parse_qs(url.query).get("token", [""])[0]
        given = hashlib.sha256(token.encode()).hexdigest()
        if not token or not hmac.compare_digest(given, read_meta(key).get("delete_hash", "")):
            return self.send(403, b"wrong delete token\n")
        remove(key)
        self.send(200, b'{"deleted": true}', "application/json")

    do_HEAD = do_GET

    def log_message(self, fmt, *args):
        sys.stderr.write("%s %s\n" % (self.command, self.path.split("?")[0]))


if __name__ == "__main__":
    if not TOKEN:
        sys.exit("set UPLOAD_TOKEN to a long random string first")
    sweep()
    print(f"listening on http://{HOST}:{PORT}, storing in {ROOT}")
    http.server.ThreadingHTTPServer((HOST, PORT), Handler).serve_forever()
