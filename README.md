<div align="center">

<img src="icons/sc-apps-lorgn.svg" width="128" alt="Lorgn">

# Lorgn ("lord" with an n)

Instant screenshot capture with shareable links on your own domain.

</div>

Lorgn is a fork of [Spectacle](https://invent.kde.org/plasma/spectacle) 6.3.5 for KDE Plasma.

## What sets it apart

- **One-click upload.** Launch it, drag a region, and press Upload (or Enter). The image goes to your own server and the link is on your clipboard.
- **Your own domain.** Point it at any server that accepts an upload, such as a small script or a Cloudflare Worker, and set it up in Settings > Upload. Cloudflare Access is supported. See [docs/upload-server.md](docs/upload-server.md).
- **Region first.** Launching opens the region overlay right away.
- **Expiring and deletable links.** Choose how long a link lasts, and delete an upload from the "link copied" message.
- **Screen recordings too.** Finish a recording, press Upload, and the video link is on your clipboard.
- **Runs alongside Spectacle.** It has its own name, icon and settings, so both can be installed together.

## Install

On Debian, Ubuntu and derivatives, download the `.deb` from the [latest release](https://github.com/amandoti-win/lorgn/releases/latest) and install it:

    sudo apt install ./lorgn_1.0.0_amd64.deb

Then log out and back in once, so KDE picks up Lorgn's shortcuts (Print launches it if nothing else has the key).

To build it yourself you need the Qt 6.7+ and KDE Frameworks 6.10+ development packages:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j$(nproc)
    sudo cmake --install build

**Lorgn has to be installed to take screenshots.** On Wayland, KWin only lets a program capture the screen if it is named in an installed desktop file. The package and `cmake --install` provide that file. A copy run straight from the build folder is refused with "The process is not authorized to take a screenshot".

Install `wl-clipboard` too (the package recommends it), so a copied link survives after Lorgn quits.

Tested on KDE Plasma 6 on Wayland (Debian 13). X11 and other distributions are untested.

## Credits and license

Lorgn is a fork of KDE Spectacle. Spectacle's authors and their copyright notices are kept in the source files and the About dialog. The original README is in `README.spectacle.md`.

Lorgn is GPL-3.0-or-later (`LICENSE`). Spectacle files keep their own headers, mostly LGPL-2.0-or-later, which allows this. New files are GPL-3.0-or-later.

The icon was drawn for this project and has no third-party artwork.

## AI disclosure

Built with AI assistance, directed by the maintainer.
