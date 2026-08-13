# Changelog

All notable changes made in the `sacd_extract2` fork are documented here. The
six versioned fork commits preceding this changelog are accounted for below.
Earlier project history remains available in the
[legacy changelog](archive/changelog).

## [Unreleased]

### Changed

- Move the extractor entry point, bundled getopt implementation, and CMake
  project from `tools/sacd_extract/` to the root-level `src/` directory.

## [0.4.4] - 2026-08-13

### Added

- Add a CMake-based 64-bit Windows build using MinGW-w64 and MSYS2.
- Build, test, package, checksum, and publish a self-contained Windows
  executable through GitHub Actions alongside the Linux artifact.

### Changed

- Archive the restored pre-fork changelog and legacy `readme.txt` under
  `archive/`.
- Remove the obsolete Visual Studio solution/project, compatibility headers,
  and manual Windows dependency-build instructions.

## [0.4.3] - 2026-08-13

### Fixed

- Generate per-track CUE sheets for DSF and DSDIFF output using the actual
  extracted filenames and file-relative indexes.
- Preserve first-track pregaps, respect selected-track extraction, and retain
  the single-file layout used by Edit Master output.

### Changed

- Update project and release links for the renamed `sacd_extract2` repository.

Commit: [`d86db50`](https://github.com/dev-zetta/sacd_extract2/commit/d86db5081b6abc6827b3bbcf49ef601c20ec9e8a)

## [0.4.2] - 2026-08-13

### Added

- Add Linux-friendly long-option aliases, including `--multichannel`, while
  retaining the existing short options.

### Changed

- Make command-line options position independent and accept the input source as
  a positional argument.
- Preserve compatibility with legacy invocations and `POSIXLY_CORRECT` use.
- Add regression coverage for argument ordering and aliases.

Commit: [`12946bc`](https://github.com/dev-zetta/sacd_extract2/commit/12946bcf6bfed7da57f6d3ac3c27bceda4b34552)

## [0.4.1] - 2026-08-13

### Changed

- Replace the runtime libxml2 dependency with the bundled XML writer so release
  binaries run without a matching system libxml2 installation.
- Build and test portable Ubuntu 22.04 release binaries in GitHub Actions.
- Attach the tested executable archive and checksum to GitHub releases.

### Tests

- Add XML-output regression tests and validate metadata, CUE/XML export, and
  selected-track extraction against an opt-in SACD ISO.

Commit: [`4aa0e74`](https://github.com/dev-zetta/sacd_extract2/commit/4aa0e745f0b2b2f9ff3387ae8d029b64dfc0efcd)

## [0.4.0] - 2026-08-13

### Added

- Add bounded sector and frame recovery with two retries, configurable
  per-track defect limits, ISO zero filling, and `.partial`/`.failed` output
  naming.
- Add structured session logging, explicit exit statuses, TOC fallback, and
  aggregate extraction results.
- Split reusable code into `sacd_extract_core`, vendor Unity, integrate CTest,
  and add Release plus sanitizer jobs to GitHub Actions.

### Fixed

- Fix intermittent DSF corruption caused by output buffering retaining a
  caller-owned buffer after its lifetime ended.
- Fix multichannel backup TOC copying and make damaged-area handling resilient.

### Changed

- Refocus the project on the host-native PC/Linux extractor and remove PS3
  tooling, dependencies, and documentation.
- Refresh the README, build configuration, packaging, project metadata, and
  Linux release workflow.

Commits:

- [`eae9707`](https://github.com/dev-zetta/sacd_extract2/commit/eae970776f570e4d6113757d9cab1d1e7c21247e) - fix intermittent DSF output corruption.
- [`fb764eb`](https://github.com/dev-zetta/sacd_extract2/commit/fb764ebddd2a98d5f6eb5048d3c838824f192a67) - refocus the project on the Linux extractor.
- [`df7a76c`](https://github.com/dev-zetta/sacd_extract2/commit/df7a76c95cf09cd768c18c504119a87c421f85d9) - publish version 0.4.0 documentation and release metadata.

[0.4.3]: https://github.com/dev-zetta/sacd_extract2/compare/v0.4.2...v0.4.3
[0.4.2]: https://github.com/dev-zetta/sacd_extract2/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/dev-zetta/sacd_extract2/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/dev-zetta/sacd_extract2/compare/c9af7d4...v0.4.0
[0.4.4]: https://github.com/dev-zetta/sacd_extract2/compare/v0.4.3...v0.4.4
[Unreleased]: https://github.com/dev-zetta/sacd_extract2/compare/v0.4.4...HEAD
