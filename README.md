# Lorgn

Lorgn ("lord" with an n) is a fork of [KDE Spectacle](https://invent.kde.org/plasma/spectacle) 6.3.5 for KDE Plasma.
It adds one thing: the Copy button can upload the screenshot to a server you run and put the link on the clipboard.

Pick a region, click Copy, paste the link.

Version 0.1.0. Early and unfinished.

## What it does

- **Upload on Copy.** With the option on (Settings > Upload), the Copy button and Ctrl+C in the region overlay upload the image and copy the link instead of the image.
  With it off, Lorgn behaves like Spectacle.
- **Your own server.** You give it a URL, the HTTP method, headers and how to read the link out of the response.
  The settings page also has fields for an Authorization header and a Cloudflare Access service token.
- **Settings page.** Settings > Upload edits `~/.config/lorgn/upload.json` (mode 600) and has a "Save and test upload" button.
- **Coexists with Spectacle.** Own binary (`lorgn`), desktop ID, D-Bus names and config file. No default global shortcuts are set.

Not done yet: expiring links, delete links, client-side encryption.

## Build

Needs Qt 6.7+ and KDE Frameworks 6.10+ (Debian 13 has both).

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
    cmake --build build -j$(nproc)
    cmake --install build

With a per-user prefix such as `~/.local`, systemd does not look in `~/.local/lib/systemd/user`, so starting Lorgn
from the app menu or dock fails ("Unit app-win.amandoti.lorgn.service not found"). Copy the unit to where systemd looks:

    mkdir -p ~/.config/systemd/user
    cp ~/.local/lib/systemd/user/app-win.amandoti.lorgn.service ~/.config/systemd/user/
    systemctl --user daemon-reload

On Wayland, KWin only lets an app take screenshots if it is listed in an installed desktop file
that names the restricted interface. The install step provides that file. A binary run straight from
the build directory is refused ("The process is not authorized to take a screenshot").

## Upload server

Lorgn needs a server to upload to. [docs/upload-server.md](docs/upload-server.md) covers running one yourself
(`examples/server.py`) or on Cloudflare (`examples/cloudflare-worker/`), and connecting it in Settings > Upload.

The server must accept the image over HTTP and return something the link can be built from.
Example `~/.config/lorgn/upload.json`:

    {
      "url": "https://files.example.com/api/upload",
      "method": "PUT",
      "body": "raw",
      "query": { "name": "{filename}" },
      "headers": { "Authorization": "Bearer YOUR_TOKEN" },
      "response": { "json_pointer": "/key" },
      "link": "https://files.example.com/f/{value}"
    }

`body` is `raw` or `multipart` (with `file_field`). `response` takes exactly one of `json_pointer`, `regex` or `text`.
If there is no Lorgn config, the same format at `~/.config/spectacle-uploader/config.json` is read.
Redirects are not followed.

## Status

Tested on Debian 13, KDE Plasma on Wayland, Qt 6.8, Frameworks 6.13, uploading to a Cloudflare Worker with R2:

- Region capture, Copy uploads, link pasted: worked.
- Upload settings page and test button: worked.
- The link staying on the clipboard after Lorgn quits relies on `wl-copy` (wl-clipboard) being installed.
  Before that was added the link disappeared when Lorgn exited. With `wl-copy` it is not confirmed yet.
  Without `wl-copy` it will likely still disappear if no clipboard manager is running.

Not tested: X11, other distributions, other servers, screen recording (it is Spectacle's code, unchanged; a recording
started without a system tray has no visible way to finish it), Flatpak.

## Credits and license

Lorgn is based on KDE Spectacle. Spectacle's authors and their copyright notices are kept in the source files and in
the About dialog. The original Spectacle README is kept as `README.spectacle.md`.

Lorgn is GPL-3.0-or-later (see `LICENSE`). Spectacle files keep their own license headers, mostly
LGPL-2.0-or-later, which allows this. New Lorgn files are GPL-3.0-or-later.
