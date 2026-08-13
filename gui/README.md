# sacd-extract-gui

An Electron desktop interface for the `sacd_extract2` command-line extractor.
It supports DSF and DSDIFF output, stereo and multichannel areas, DST decoding,
and CUE export on Linux and Windows.

The GUI is built and tested together with the matching extractor by this
repository's GitHub Actions workflow. Ready-to-run GUI archives are published
with tagged releases.

## Development

Install Node.js 22 or newer, build the extractor from the repository root, and
install the locked JavaScript dependencies:

```bash
cmake -S src -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
npm ci --prefix gui
npm --prefix gui test
npm --prefix gui run check
npm --prefix gui start
```

The GUI searches for `sacd_extract` (`sacd_extract.exe` on Windows) in this
order:

1. the path in `SACD_EXTRACT_PATH`;
2. the packaged application's `resources` directory;
3. the `gui` directory;
4. `build/release` and `build/sacd_extract` in this repository;
5. directories listed in `PATH`.

Use `SACD_EXTRACT_PATH` when the executable is in a different build directory:

```bash
SACD_EXTRACT_PATH=/absolute/path/to/sacd_extract npm --prefix gui start
```

## Packaging

Packaging requires the matching host-platform extractor. Point Electron Forge
to it with `SACD_EXTRACT_BINARY`:

```bash
SACD_EXTRACT_BINARY="$PWD/build/release/sacd_extract" \
  npm --prefix gui run package -- --arch=x64
```

Electron Forge copies the binary outside the ASAR archive into the packaged
application's `resources` directory. GitHub Actions performs the same process
on Ubuntu and Windows, verifies the embedded binary, then archives the complete
ready-to-run application.

## Tests

`npm --prefix gui test` verifies executable discovery, version parsing, and the
exact long-form arguments passed to the current extractor. `npm --prefix gui
run check` syntax-checks all main-process, preload, renderer, and adapter code.

## Provenance and license

The GUI is MIT licensed. See [`LICENSE`](LICENSE) and [`UPSTREAM.md`](UPSTREAM.md)
for the retained upstream attribution and adopted revision. The extractor at
the repository root remains licensed under GPL-2.0.
