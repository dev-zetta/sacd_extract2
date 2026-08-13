#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$script_dir/.." && pwd)

platform=auto
build_type=Release
build_dir=
package=0
with_gui=1
sanitizers=0

usage() {
  cat <<'EOF'
Usage: scripts/build.sh [options]

Configure, build, and test sacd_extract and its GUI on Linux or Windows.

Options:
  --platform auto|linux|windows  Target host platform (default: auto)
  --build-dir DIR               Build directory (default: build/<platform>)
  --build-type TYPE             CMake build type (default: Release)
  --package                     Create CLI and ready-to-run GUI archives
  --skip-gui                    Build and test only the command-line tool
  --sanitizers                  Enable AddressSanitizer and UBSan (Linux only)
  -h, --help                    Show this help

The Windows build must run in an MSYS2 MinGW64 shell. Dependencies are checked
before configuration and missing tools are reported with platform-specific
installation guidance.
EOF
}

fail() {
  printf 'error: %s\n' "$*" >&2
  exit 2
}

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    missing_commands+=("$1")
  fi
}

while (($# > 0)); do
  case "$1" in
    --platform)
      (($# >= 2)) || fail '--platform requires a value'
      platform=$2
      shift 2
      ;;
    --build-dir)
      (($# >= 2)) || fail '--build-dir requires a value'
      build_dir=$2
      shift 2
      ;;
    --build-type)
      (($# >= 2)) || fail '--build-type requires a value'
      build_type=$2
      shift 2
      ;;
    --package)
      package=1
      shift
      ;;
    --skip-gui)
      with_gui=0
      shift
      ;;
    --sanitizers)
      sanitizers=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "unknown option: $1"
      ;;
  esac
done

host_name=$(uname -s)
if [[ $platform == auto ]]; then
  case "$host_name" in
    Linux*) platform=linux ;;
    MINGW*|MSYS*) platform=windows ;;
    *) fail "unsupported host: $host_name" ;;
  esac
fi

case "$platform" in
  linux)
    [[ $host_name == Linux* ]] || fail 'Linux builds must run on a Linux host'
    executable_name=sacd_extract
    cmake_generator=()
    cmake_platform_options=()
    cli_archive=sacd_extract-linux-x86_64.tar.gz
    gui_archive=sacd-extract-gui-linux-x86_64.tar.gz
    gui_package_name=sacd-extract-gui-linux-x64
    ;;
  windows)
    [[ $host_name == MINGW* || $host_name == MSYS* ]] || \
      fail 'Windows builds must run in an MSYS2 MinGW64 shell'
    [[ ${MSYSTEM:-} == MINGW64 ]] || \
      fail 'Windows builds require the MSYS2 MINGW64 environment'
    executable_name=sacd_extract.exe
    cmake_generator=(-G Ninja)
    cmake_platform_options=(-DSACD_WINDOWS_STATIC=ON)
    cli_archive=sacd_extract-windows-x86_64.zip
    gui_archive=sacd-extract-gui-windows-x86_64.zip
    gui_package_name=sacd-extract-gui-win32-x64
    ;;
  *)
    fail "unsupported platform: $platform"
    ;;
esac

if ((sanitizers)) && [[ $platform != linux ]]; then
  fail '--sanitizers is currently supported only for Linux builds'
fi

if [[ -z $build_dir ]]; then
  build_dir="$repo_root/build/$platform"
elif [[ $build_dir != /* && ! $build_dir =~ ^[A-Za-z]:[/\\] ]]; then
  build_dir="$repo_root/$build_dir"
fi

missing_commands=()
require_command cmake
require_command ctest
require_command cmp
require_command git
require_command sha256sum

if [[ $platform == linux ]]; then
  require_command cc
  require_command ldd
  require_command tar
else
  require_command gcc
  require_command ninja
  require_command objdump
fi

if ((with_gui)); then
  require_command node
  require_command npm
fi

if ((${#missing_commands[@]} > 0)); then
  printf 'Missing required commands: %s\n' "${missing_commands[*]}" >&2
  if [[ $platform == linux ]]; then
    printf '%s\n' \
      'Debian/Ubuntu: sudo apt-get install build-essential cmake git nodejs npm' >&2
  else
    printf '%s\n' \
      'MSYS2 MINGW64: pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-libiconv git' >&2
    printf '%s\n' 'Install Node.js 22 or newer separately.' >&2
  fi
  exit 2
fi

printf 'Building for %s in %s\n' "$platform" "$build_dir"

cmake_build_options=()
if ((sanitizers)); then
  cmake_build_options+=(
    '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'
    '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
  )
fi

cmake \
  -S "$repo_root/src" \
  -B "$build_dir" \
  "${cmake_generator[@]}" \
  -DCMAKE_BUILD_TYPE="$build_type" \
  -DBUILD_TESTING=ON \
  "${cmake_platform_options[@]}" \
  "${cmake_build_options[@]}"
cmake --build "$build_dir" --parallel
if ((sanitizers)); then
  ASAN_OPTIONS=detect_leaks=0 \
    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
    ctest --test-dir "$build_dir" --output-on-failure
else
  ctest --test-dir "$build_dir" --output-on-failure
fi

extractor="$build_dir/$executable_name"
[[ -f $extractor ]] || fail "built extractor not found: $extractor"
if ((sanitizers)); then
  ASAN_OPTIONS=detect_leaks=0 \
    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
    "$extractor" --version
  ASAN_OPTIONS=detect_leaks=0 \
    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
    "$extractor" --help >/dev/null
else
  "$extractor" --version
  "$extractor" --help >/dev/null
fi

if [[ $platform == linux ]]; then
  if ldd "$extractor" | grep -q libxml; then
    fail 'unexpected libxml runtime dependency'
  fi
else
  imports_file="$build_dir/windows-imports.txt"
  objdump -p "$extractor" >"$imports_file"
  if grep -Eiq 'DLL Name: (libiconv|libintl|libwinpthread|libgcc)' \
      "$imports_file"; then
    fail 'unexpected MinGW runtime dependency'
  fi
fi

if ((with_gui)); then
  npm ci --prefix "$repo_root/gui" --no-audit
  npm --prefix "$repo_root/gui" test
  npm --prefix "$repo_root/gui" run check
fi

if ((!package)); then
  printf 'Build and tests completed successfully.\n'
  exit 0
fi

package_root="$repo_root/build/package-$platform"
dist_root="$repo_root/build/dist"
cmake -E remove_directory "$package_root"
cmake -E make_directory "$package_root/sacd_extract" "$dist_root"
cmake -E copy "$extractor" "$package_root/sacd_extract/$executable_name"
cmake -E copy \
  "$repo_root/README.md" \
  "$repo_root/CHANGELOG.md" \
  "$repo_root/COPYING" \
  "$package_root/sacd_extract"

if [[ $platform == linux ]]; then
  (
    cd "$package_root"
    cmake -E tar czf "$dist_root/$cli_archive" \
      --format=gnutar \
      -- sacd_extract
  )
else
  (
    cd "$package_root"
    cmake -E tar cf "$dist_root/$cli_archive" --format=zip sacd_extract
  )
fi

if ((with_gui)); then
  extractor_for_node=$extractor
  if [[ $platform == windows ]]; then
    extractor_for_node=$(cygpath -w "$extractor")
  fi

  SACD_EXTRACT_BINARY="$extractor_for_node" \
    npm --prefix "$repo_root/gui" run package -- --arch=x64

  gui_package="$repo_root/gui/out/$gui_package_name"
  embedded_extractor="$gui_package/resources/$executable_name"
  [[ -f $embedded_extractor ]] || \
    fail "packaged extractor not found: $embedded_extractor"
  cmp "$extractor" "$embedded_extractor" || \
    fail 'packaged extractor differs from the tested executable'

  cmake -E copy "$repo_root/README.md" "$gui_package/PROJECT-README.md"
  cmake -E copy "$repo_root/CHANGELOG.md" "$gui_package/CHANGELOG.md"
  cmake -E copy "$repo_root/COPYING" "$gui_package/COPYING"
  cmake -E copy "$repo_root/gui/LICENSE" "$gui_package/GUI-LICENSE"
  cmake -E copy "$repo_root/gui/UPSTREAM.md" "$gui_package/GUI-UPSTREAM.md"

  if [[ $platform == linux ]]; then
    (
      cd "$repo_root/gui/out"
      cmake -E tar czf "$dist_root/$gui_archive" \
        --format=gnutar \
        -- "$gui_package_name"
    )
  else
    (
      cd "$repo_root/gui/out"
      cmake -E tar cf "$dist_root/$gui_archive" \
        --format=zip "$gui_package_name"
    )
  fi
fi

(
  cd "$dist_root"
  sha256sum "$cli_archive" >"$cli_archive.sha256"
  if ((with_gui)); then
    sha256sum "$gui_archive" >"$gui_archive.sha256"
  fi
)

printf 'Packages written to %s\n' "$dist_root"
