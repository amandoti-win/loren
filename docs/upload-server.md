# Setting up the upload server

Lorgn does not host anything. When you upload, it sends the image to a server you run and copies the link that server
gives back. The link is on your own domain.

You need two things: a server that takes an upload and serves the file back, and the Lorgn upload settings pointing at it.

## What the server has to do

Lorgn sends the image as an HTTP request and reads the link out of the reply. The examples in this repo use this API:

    PUT /upload?name=shot.png&expires=86400   Authorization: Bearer <token>   body: the PNG bytes
    ->  {"key": "AbC123xY.png", "delete_token": "...", "expires_at": 1790000000}

    GET /f/AbC123xY.png                       ->  the image (public, no login)
    DELETE /f/AbC123xY.png?token=<delete_token>  ->  {"deleted": true}

`expires` and the delete token are optional. Without `expires` the file is kept until you delete it. When Lorgn is set
to expire links it adds `expires=<seconds>` to the upload, and the server deletes the file after that time. A deleted
or expired link answers 404. The delete token is a secret: whoever has it can delete the file, so it is never part of
the public link.

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

The example server supports expiry and delete tokens. It removes expired files on start-up and after each upload, and it
never serves an expired file. It limits each file to 50 MB (`MAX_MB`).
Treat it as a starting point. Screen recordings are much larger than screenshots, so raise `MAX_MB` if you upload them.

## Option B: Cloudflare (Worker and R2, no server to run)

`examples/cloudflare-worker/` is a Worker that stores uploads in an R2 bucket and serves them at `/f/<key>`, with the same
API as above. You need a Cloudflare account with R2 enabled. Node.js is needed for `wrangler`.

    cd examples/cloudflare-worker
    npx wrangler r2 bucket create shots-files
    npx wrangler deploy
    npx wrangler secret put UPLOAD_TOKEN

Paste a long random token when asked (`openssl rand -base64 32` makes one) and keep a copy for Lorgn.
The Worker is then live at `https://shots.<your-subdomain>.workers.dev`.

The Worker supports expiry and delete tokens. An hourly cron trigger (already in `wrangler.jsonc`) deletes expired
files, and an expired file stops being served as soon as it expires. Screen recordings are uploaded the same way, and the Worker rejects anything over `MAX_MB` (90 by default; Cloudflare also
caps request bodies at 100 MB on the free plan).

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

To use expiring and deletable links, also fill in:

| Field | Value for the examples above |
| --- | --- |
| Links expire after | Never, 1 hour, 1 day, 7 days or 30 days |
| Delete token from response | `/delete_token` |
| Delete link | `https://files.example.com/f/{value}?token={delete}` |

With those set, the "link copied" message has a **Delete** button that removes the file from your server. The message
stays for 30 seconds. Every upload is also written to `~/.local/state/lorgn/uploads.jsonl` (readable only by you) with
its link and delete address, so you can delete it later, for example with
`curl -X DELETE '<delete_url from the file>'`.

Settings are saved to `~/.config/lorgn/upload.json`, readable only by you, because it can hold your token.
You can also write that file by hand:

    {
      "url": "https://files.example.com/upload",
      "method": "PUT",
      "body": "raw",
      "query": { "name": "{filename}" },
      "headers": { "Authorization": "Bearer YOUR_TOKEN" },
      "response": { "json_pointer": "/key" },
      "link": "https://files.example.com/f/{value}",
      "expires": 86400,
      "delete_pointer": "/delete_token",
      "delete_link": "https://files.example.com/f/{value}?token={delete}"
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
| `expires` | `0` | seconds until the server should delete the upload; sent as `expires=<seconds>`. `0` means never. Also available as `{expires}` in `query` and `headers` |
| `delete_pointer` | none | JSON pointer to the delete token in the server's reply |
| `delete_link` | none | template for the delete address; `{value}` is the link value and `{delete}` is the delete token |

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
- Expiry and delete: the example server was tested with an expiry, a wrong and a right delete token, and an expired file.
  Lorgn's uploader was tested against it for the expiry, the delete address, deleting, and the history file.
- The Worker's expiry and delete code was run in Node against a fake R2 bucket, all routes and the hourly cleanup. It has
  not run on real Cloudflare.
- The Worker in `examples/cloudflare-worker/` has not been deployed from this repo by me. It implements the same API as
  `server.py`, and a private Worker with the same upload API was used with Lorgn.
- Cloudflare Access with a service token was used in the same private setup.
