# Third-party software

Custom Skin Studio uses the following libraries. Their licenses apply to their parts.

| Library | License | Use |
|---|---|---|
| [Qt 6](https://www.qt.io) (Core, Gui, Qml, Quick, Quick 3D, Multimedia) | LGPL-3.0 (dynamically linked; source: https://download.qt.io) | UI, 3D view, audio |
| [Zstandard](https://github.com/facebook/zstd) 1.5.6 | BSD-3-Clause | WAD compression |
| [xxHash](https://github.com/Cyan4973/xxHash) 0.8.2 | BSD-2-Clause | WAD path hashes |
| [miniz](https://github.com/richgel999/miniz) 3.0.2 | MIT | .fantome (zip) |
| FFmpeg (shipped with Qt Multimedia) | LGPL-2.1+ | decoding audio files |

Qt is used under the LGPL: the Qt libraries are shipped as separate shared libraries / frameworks and can be replaced
by the user. To rebuild against your own Qt, see "Build from source" in README.md.

League of Legends is a trademark of Riot Games, Inc. This project is not endorsed by or affiliated with Riot Games.
No Riot Games files are included; the app reads the user's own game installation at runtime.
