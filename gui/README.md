# sacd-extract-gui

An Electron desktop interface for the `sacd_extract2` command-line extractor.
It supports DSF and DSDIFF output, stereo and multichannel areas, DST decoding,
and CUE export on Linux and Windows.

The GUI is built and tested together with the matching extractor by this
repository's GitHub Actions workflow. Ready-to-run GUI archives are published
with tagged releases.

## Development

Install Node.js 22 or newer and use the repository's shared build entry point:

```bash
scripts/build.sh
npm --prefix gui start
```

The GUI searches for `sacd_extract` (`sacd_extract.exe` on Windows) in this
order:

1. the path in `SACD_EXTRACT_PATH`;
2. the packaged application's `resources` directory;
3. the `gui` directory;
4. the platform-default `build/linux` or `build/windows` directory, followed by
   the legacy `build/release` and `build/sacd_extract` directories;
5. directories listed in `PATH`.

Use `SACD_EXTRACT_PATH` when the executable is in a different build directory:

```bash
SACD_EXTRACT_PATH=/absolute/path/to/sacd_extract npm --prefix gui start
```

## Packaging

The shared script packages the matching host-platform extractor and GUI:

```bash
scripts/build.sh --package
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
