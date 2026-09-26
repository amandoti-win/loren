# Lorgn

Instant screenshot capture with shareable links on your own domain.

Lorgn is a fork of [Spectacle](https://invent.kde.org/plasma/spectacle) 6.3.5 for KDE Plasma. You launch it, drag a
region, and press Upload. The image goes to a server you run and the link ends up on your clipboard.

The name comes from the lorgnette, spectacles on a handle. It is pronounced like "lord" with an n.

Version 0.1.0. Early, and not everything is tested (see Status).

## What's different from Spectacle

- **Upload.** The Copy button becomes Upload once you turn it on in Settings > Upload. Enter does the same in the region
  overlay. With it off, Copy only copies the image.
- **Region first.** Launching opens the region overlay straight away.
- **Your server.** Settings > Upload takes a URL, method, headers and how to read the link out of the reply,
  including a Cloudflare Access service token. There is a "Save and test upload" button.
- **Own identity.** Its own binary (`lorgn`), desktop ID, D-Bus names, config and icon, so it runs next to Spectacle.
  No global shortcuts are set by default.
- **Trimmed.** No handbook, and the KDE-specific share targets (KDE Connect, Phabricator, Review Board) are hidden.
- **Recording fixes.** Screen recording needed upstream fixes that came after 6.3.5 to work with current KPipeWire.
  They are included, and Lorgn keeps its window up while recording so Finish recording stays reachable.

Not done: expiring links, delete links, client-side encryption.

## Install

Needs Qt 6.7+ and KDE Frameworks 6.10+. On Debian 13:

    sudo apt install build-essential cmake extra-cmake-modules gettext qt6-base-dev qt6-base-private-dev \
      qt6-declarative-dev qt6-declarative-private-dev qt6-wayland-dev qt6-wayland-private-dev qt6-multimedia-dev \
      qt6-tools-dev libkf6coreaddons-dev libkf6widgetsaddons-dev libkf6dbusaddons-dev libkf6notifications-dev \
      libkf6config-dev libkf6i18n-dev libkf6kio-dev libkf6windowsystem-dev libkf6globalaccel-dev libkf6xmlgui-dev \
      libkf6guiaddons-dev libkirigami-dev libkf6statusnotifieritem-dev libkf6prison-dev libkf6crash-dev \
      libkf6purpose-dev libkpipewire-dev liblayershellqtinterface-dev plasma-wayland-protocols libwayland-dev \
      libopencv-dev libxcb-xfixes0-dev libxcb-image0-dev libxcb-util-dev libxcb-cursor-dev libxcb-randr0-dev \
      libxcb-shape0-dev

Build and install to your home directory:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
    cmake --build build -j$(nproc)
    cmake --install build

Two extra steps for a `~/.local` install:

- systemd does not look in `~/.local/lib/systemd/user`, so starting Lorgn from the app menu fails until you copy the unit:

      mkdir -p ~/.config/systemd/user
      cp ~/.local/lib/systemd/user/app-win.amandoti.lorgn.service ~/.config/systemd/user/
      systemctl --user daemon-reload

- Install `wl-clipboard` (`wl-copy`). Without it the copied link can vanish when Lorgn exits.

On Wayland, KWin only lets an app take screenshots if it is named in an installed desktop file. The install step
provides that file, so a binary run straight from the build folder is refused.

## Upload server

Lorgn needs somewhere to upload to. [docs/upload-server.md](docs/upload-server.md) covers running a small server yourself
(`examples/server.py`) or on Cloudflare (`examples/cloudflare-worker/`), and connecting it in Settings > Upload.

Settings are saved to `~/.config/lorgn/upload.json`, readable only by you:

    {
      "url": "https://files.example.com/upload",
      "method": "PUT",
      "body": "raw",
      "query": { "name": "{filename}" },
      "headers": { "Authorization": "Bearer YOUR_TOKEN" },
      "response": { "json_pointer": "/key" },
      "link": "https://files.example.com/f/{value}"
    }

## Status

Developed on Debian 13, KDE Plasma on Wayland, Qt 6.8, Frameworks 6.13.

Tested there:
- Region capture, Upload to a Cloudflare Worker with R2, link pasted.
- Settings > Upload and the test button.
- Launching from the app menu.
- The example `server.py` against Lorgn's uploader.

Not confirmed:
- The link surviving after Lorgn quits.
- Finishing a screen recording. The recorder starts, but finishing and saving is not confirmed.
- The example Cloudflare Worker deployed from this repo.

Not tested: X11, other distributions, Flatpak.

## Credits and license

Lorgn is a fork of KDE Spectacle. Spectacle's authors and their copyright notices are kept in the source files and the
About dialog. The original README is in `README.spectacle.md`.

Lorgn is GPL-3.0-or-later (`LICENSE`). Spectacle files keep their own headers, mostly LGPL-2.0-or-later, which allows
this. New files are GPL-3.0-or-later.

The icon was drawn for this project and has no third-party artwork.

## AI disclosure

Built with AI assistance, directed by the maintainer.
