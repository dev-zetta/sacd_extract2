# Testing sacd_extract2

The shared build script runs the normal production and GUI test suites:

```bash
scripts/build.sh
```

Use `--skip-gui` when only the C extractor and library tests are needed.

## C tests

Unity 2.6.1 is vendored under `tests/vendor/unity`, so the C unit tests do not
fetch dependencies. A manual Debug build can be run with:

```bash
cmake -S src -B build/tests \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

The suite covers recovery, output status and paths, logging, DSD/DST frame
handling, CUE/XML output, TOC fallback, and CLI parsing/configuration behavior.

## GUI tests

The GUI uses Node's built-in test runner for executable discovery, argument
generation, version parsing, and exit-status mapping. It also syntax-checks the
Electron main process, preload, renderer, and adapter:

```bash
npm ci --prefix gui
npm --prefix gui test
npm --prefix gui run check
```

## Sanitizers

Run the C suite under AddressSanitizer and UndefinedBehaviorSanitizer with:

```bash
scripts/build.sh --platform linux --build-dir build/sanitizers \
  --build-type Debug --sanitizers --skip-gui
```

Leak detection is disabled for this job because LeakSanitizer is unavailable in
some supported execution environments; address and undefined-behavior checks
remain enabled.

## Optional SACD ISO integration tests

Point CMake at a local SACD image to register read-only metadata, CUE/XML, and
selected-track checks:

```bash
cmake -S src -B build/integration \
  -DBUILD_TESTING=ON -DSACD_TEST_ISO="/path/to/Album.iso"
cmake --build build/integration --parallel
ctest --test-dir build/integration -L integration --output-on-failure
```

The image is referenced in place and is never copied into the build, repository,
or test artifacts. Local images and extraction samples can be kept under the
ignored `work/` directory.

## Continuous integration

GitHub Actions calls `scripts/build.sh` for Ubuntu Release, Windows Release, and
Linux ASan/UBSan jobs. It uploads tested CLI and GUI archives with checksums.
When a version tag is pushed, the tag workflow reuses artifacts from the
successful branch build for the same commit instead of compiling everything a
second time.
