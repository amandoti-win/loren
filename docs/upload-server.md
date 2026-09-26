# Setting up the upload server

Lorgn does not host anything. When you upload, it sends the image to a server you run and copies the link that server
gives back. The link is on your own domain.

You need two things: a server that takes an upload and serves the file back, and the Lorgn upload settings pointing at it.

## What the server has to do

Lorgn sends the image as an HTTP request and reads the link out of the reply. The examples in this repo use this API:

    PUT /upload?name=shot.png        Authorization: Bearer <token>     body: the PNG bytes
    ->  {"key": "AbC123xY.png"}

    GET /f/AbC123xY.png              ->  the image (public, no login)

Anyone with a link can view the file. Only someone with the token can upload. Any server that accepts an image and
returns something you can build a link from will work. See "Other servers" below.

## Option A: your own server

`examples/server.py` is a small standard-library Python server. It needs Python 3 and nothing else.

1. Copy `server.py` to the machine and make a token:

       python3 -c "import secrets; print(secrets.token_urlsafe(32))"

2. Run it (keep it alive with systemd, Docker or whatever you use):

       UPLOAD_TOKEN=<the token> UPLOAD_DIR=/srv/uploads ./server.py

   It listens on `127.0.0.1:8080` only.

3. Put a reverse proxy in front of it for HTTPS on your domain. With Caddy:

       files.example.com {
           reverse_proxy 127.0.0.1:8080
       }

   Point a DNS record for `files.example.com` at the machine. Ports 80 and 443 must be reachable from the internet.
   On many home connections they are not; use Option B instead.

The example server stores files forever, has no delete or expiry, and limits each file to 50 MB (`MAX_MB`).
Treat it as a starting point.

## Option B: Cloudflare (Worker and R2, no server to run)

`examples/cloudflare-worker/` is a Worker that stores uploads in an R2 bucket and serves them at `/f/<key>`, with the same
API as above. You need a Cloudflare account with R2 enabled. Node.js is needed for `wrangler`.

    cd examples/cloudflare-worker
    npx wrangler r2 bucket create shots-files
    npx wrangler deploy
    npx wrangler secret put UPLOAD_TOKEN

Paste a long random token when asked (`openssl rand -base64 32` makes one) and keep a copy for Lorgn.
The Worker is then live at `https://shots.<your-subdomain>.workers.dev`.

To use your own domain, which must be on your Cloudflare account, add this to `wrangler.jsonc` before deploying:

    "routes": [{ "pattern": "shots.example.com", "custom_domain": true }]

### Protecting uploads with Cloudflare Access instead of a token (optional)

If you put the whole hostname behind Cloudflare Access, so a login page guards the site, uploads from Lorgn need a
service token:

1. In Zero Trust, create a service token (Access controls, Service credentials).
2. On the Access application for your hostname, add a policy with action **Service Auth** that includes that token.
3. Add a second Access application for the path `f/*` on the same hostname with a **Bypass** policy for everyone,
   so shared links open without a login.
4. In Lorgn's Upload settings, fill in **Access client ID** and **Access client secret** with the token's values.

Lorgn sends its own `User-Agent`, because Cloudflare blocks empty and library-default ones.

## Connect Lorgn to it

Open Lorgn, go to Settings, then Upload, and fill in:

| Field | Value for the examples above |
| --- | --- |
| Server URL | `https://files.example.com/upload` (or your Worker's URL plus `/upload`) |
| Method | `PUT` |
| Send image as | Raw image |
| Authorization header | `Bearer <your token>` |
| Read link from response as | JSON pointer |
| Response value | `/key` |
| Link template | `https://files.example.com/f/{value}` |

Click **Save and test upload**. It uploads a small test image and shows the link, or the error.
Then tick **Copy button uploads and copies the link**. The Copy button now reads Upload.

Settings are saved to `~/.config/lorgn/upload.json`, readable only by you, because it can hold your token.
You can also write that file by hand:

    {
      "url": "https://files.example.com/upload",
      "method": "PUT",
      "body": "raw",
      "query": { "name": "{filename}" },
      "headers": { "Authorization": "Bearer YOUR_TOKEN" },
      "response": { "json_pointer": "/key" },
      "link": "https://files.example.com/f/{value}"
    }

For Access, put `CF-Access-Client-Id` and `CF-Access-Client-Secret` in `headers` instead.

### Config keys

| Key | Default | Meaning |
| --- | --- | --- |
| `url` | required | `http` or `https` address to send the image to |
| `method` | `POST` | `POST`, `PUT` or `PATCH` |
| `body` | `multipart` | `multipart` form upload, or `raw` (image bytes as the body) |
| `file_field` | `file` | form field name for `multipart` |
| `query` | `{}` | query parameters |
| `headers` | `{}` | extra request headers, such as auth |
| `response` | `{"text": true}` | how to find the link: exactly one of `json_pointer`, `regex` (first group, else the whole match) or `text` (the whole body) |
| `link` | `{value}` | template for the final link |
| `timeout` | `300` | seconds |

`query` and `headers` values and `link` can use `{filename}` and `{mime}`; `link` can also use `{value}`, which is what
`response` extracted. Redirects are not followed, so a login page in front of your server shows up as an error instead
of a false success.

### Other servers

Any host works if you can describe it in those keys. Two examples:

A form upload that answers with the link as plain text:

    { "url": "https://up.example.com/upload", "file_field": "file" }

A form upload whose reply is JSON like `{"data": {"url": "..."}}`:

    {
      "url": "https://up.example.com/upload",
      "headers": { "Authorization": "Bearer YOUR_TOKEN" },
      "response": { "json_pointer": "/data/url" }
    }

## What has been tested

- `examples/server.py` with Lorgn's uploader: the upload worked, the returned link served the image, and a wrong token
  produced a clear "HTTP 401, check your credentials" message.
- The Worker in `examples/cloudflare-worker/` has not been deployed from this repo by me. It implements the same API as
  `server.py`, and a private Worker with the same upload API was used with Lorgn.
- Cloudflare Access with a service token was used in the same private setup.
