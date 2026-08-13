# sacd-extract-gui

A Tauri 2 desktop interface for the `sacd_extract2` command-line extractor.
It supports DSF, DSDIFF, and 24-bit PCM FLAC output, stereo and multichannel
areas, DST decoding, and CUE export on Linux, Apple Silicon macOS, and Windows.

The GUI is built and tested together with the matching extractor by this
repository's GitHub Actions workflow. Tagged releases publish a Linux AppImage,
Apple Silicon macOS DMG, and Windows installer.

Run the Linux package without installation:

```bash
chmod +x sacd-extract-gui-linux-x86_64.AppImage
./sacd-extract-gui-linux-x86_64.AppImage
```

## Development

Install Node.js 22 or newer, the stable Rust toolchain, and the Tauri platform
dependencies listed in [`docs/building.md`](../docs/building.md). Then use the
repository's shared build entry point:

```bash
scripts/build.sh
npm --prefix gui start
```

The shared script builds the extractor and prepares it under
`src-tauri/binaries` using Tauri's required target-triple filename. Packaged
applications always execute that bundled sidecar. For development, set
`SACD_EXTRACT_PATH` to test a different extractor explicitly:

```bash
SACD_EXTRACT_PATH=/absolute/path/to/sacd_extract npm --prefix gui start
```

## Packaging

The shared script packages the matching host-platform extractor and GUI:

```bash
scripts/build.sh --package
```

Tauri embeds the prepared extractor as an external sidecar. The build script
verifies that this input is byte-identical to the extractor that passed CTest,
then produces `sacd-extract-gui-linux-x86_64.AppImage` on Linux,
`sacd-extract-gui-macos-arm64.dmg` on Apple Silicon macOS, or
`sacd-extract-gui-windows-x86_64-setup.exe` on Windows. The community macOS
package uses an ad-hoc signature and is not Apple-notarized.

## Tests

`npm --prefix gui test` runs the Rust backend tests for version parsing, output
classification, file recognition, and the exact long-form extractor arguments.
`npm --prefix gui run check` syntax-checks the Tauri bridge and renderer.

## Provenance and license

The GUI is MIT licensed. See [`LICENSE`](LICENSE) and [`UPSTREAM.md`](UPSTREAM.md)
for the retained upstream attribution and adopted revision. The extractor at
the repository root remains licensed under GPL-2.0.
