## Current release: 0.1.87

[Download NEXAVERSE](https://github.com/NEXARYSE/nexaverse-sony-edition/releases/latest) · [0.1.87 notes](releases/0.1.87/release-notes.md)

The GTA VI entry uses the experimental NEXA GTA hardware runtime. Link your supported local Xbox executable and assets to enable Play. Shader/pipeline preparation has a logo-colored percentage bar; gameplay launches borderless and F3 opens settings. World rendering and performance are still incomplete; this is not a fully playable GTA VI release. Runtime source, portable files and license notices accompany the release.

# NEXAVERSE Sony Edition

Source code for the NEXAVERSE desktop launcher and its community-service worker.

## Included

- Electron main-process, preload, and renderer source
- Launcher game catalogue, trophy mappings, Steam sync, Discord linking, updates, and runtime adapters
- Cloudflare community-service worker source and tests

## Deliberately excluded

This repository does not distribute game files, game images, videos, emulator binaries, firmware, keys, user data, credentials, build outputs, or `node_modules`.

## Development

Use a current Windows installation of Node.js and Electron. Install the project dependencies, then run the launcher through Electron. Release packaging additionally requires the Windows Electron build toolchain and the locally supplied runtime resources referenced by the packaging configuration.

`community-config.json` is a deployment template. Set its `origin` to your own deployed community-service endpoint before using online features.

NEXAVERSE is an independent launcher. Users must provide and link software they are legally entitled to use.
