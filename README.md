# sacd_extract

`sacd_extract` is a command-line tool for inspecting readable SACD images and
extracting their audio and metadata on Linux. It can write stereo or
multichannel tracks as DSF or DSDIFF, decode DST during extraction, export an
Edit Master file, and generate CUE/XML metadata.

This repository contains the host-native extractor. It does not authenticate
or decrypt physical SACD media, so local input must already be available as a
readable SACD ISO image. A compatible network source can also be supplied as
`host:port`.

## Features

- Stereo and multichannel extraction
- DSF, DSDIFF, DSDIFF Edit Master, and raw ISO output
- DST-to-DSD conversion
- Per-track selection and consecutive-track concatenation
- CUE and XML metadata export
- Configurable ID3v2.3 and ID3v2.4 tagging
- Optional output directories, performer naming, pause handling, and DSF
  padding control

## Build on Linux

Install a C compiler, CMake 3.16 or newer, and the libxml2 development files.
For Debian and Ubuntu:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake libxml2-dev
```

Configure and build out of tree:

```bash
cmake -S tools/sacd_extract -B build/sacd_extract \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/sacd_extract --parallel
```

The executable is written to `build/sacd_extract/sacd_extract`. Verify the
build with:

```bash
build/sacd_extract/sacd_extract --version
build/sacd_extract/sacd_extract --help
```

## Usage

Print disc and track metadata without extracting audio:

```bash
sacd_extract -P -i "Album.iso"
```

Extract all stereo tracks as DSF:

```bash
sacd_extract -2 -s -i "Album.iso"
```

Extract all multichannel tracks as DSF:

```bash
sacd_extract -m -s -i "Album.iso"
```

Extract selected stereo tracks and convert DST to DSDIFF:

```bash
sacd_extract -2 -p -c -t 1,5,13 -i "Album.iso"
```

Use `-y DIR` to choose the DSF/DSDIFF output directory and `-o DIR` for raw
ISO or DSDIFF Edit Master output.

### Common options

| Option | Purpose |
| --- | --- |
| `-2` | Select the two-channel area (default) |
| `-m` | Select the multichannel area |
| `-s` | Write individual DSF tracks |
| `-p` | Write individual DSDIFF tracks |
| `-e` | Write a DSDIFF Edit Master |
| `-c` | Convert DST-compressed audio to DSD |
| `-t LIST` | Extract selected tracks, such as `1,5,13` |
| `-k` | Concatenate consecutive selected tracks |
| `-C` | Export CUE and XML metadata |
| `-P` | Print disc information without extracting |
| `-i INPUT` | Read an ISO, device, or compatible `host:port` source |

Run `sacd_extract --help` for the complete option list.

## Configuration

An optional `sacd_extract.cfg` in the current working directory can set
defaults. Command-line naming and pause options are overridden when their
matching config entries are present.

```ini
artist=0
performer=0
pauses=0
nopad=0
concatenate=0
id3tag=4
logging=0
```

`id3tag` accepts `0` (disabled), `1` or `2` (ID3v2.3 UTF-16, full or minimal),
`3` (ID3v2.3 ISO-8859-1), and `4` or `5` (ID3v2.4 UTF-8, full or minimal).
Boolean settings accept `0`/`1` or `no`/`yes`.

## Output integrity

Extracted files should be decoded or checksummed before the source image is
discarded. The investigation and fix for the former intermittent 1 MiB DSF
corruption issue is documented in
[`docs/dsf-output-corruption-investigation.md`](docs/dsf-output-corruption-investigation.md).

## Continuous integration

The GitHub Actions workflow builds the Release configuration on Ubuntu, runs
the version and help smoke checks, and publishes the executable in a compressed
Linux artifact.

## License

This project is distributed under the GNU General Public License version 2.
See [`COPYING`](COPYING).
