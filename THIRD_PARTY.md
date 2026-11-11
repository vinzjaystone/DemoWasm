# Third-party components

No third-party source, downloaded artwork, or binary dependency is checked in.

- **Raylib 5.5**, by Ramon Santamaria and contributors: [source and zlib/libpng license](https://github.com/raysan5/raylib/tree/5.5). CMake fetches the pinned release archive and verifies its SHA-256. Raylib includes its own dependency notices (GLFW, stb, glad, etc.). The game uses Raylib's embedded default font.
- **Emscripten**, The Emscripten Authors: [source and MIT / University of Illinois-NCSA licenses](https://github.com/emscripten-core/emscripten). Required only when building for the web; its compiler runtime and standard library have their own upstream notices.
- **Playwright** (optional development-only browser tests): [source and Apache-2.0 license](https://github.com/microsoft/playwright).

The game source is covered by this repository's existing GPL-3.0 license. Preserve applicable dependency notices when redistributing builds or dependency source archives.
