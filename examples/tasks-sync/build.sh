#!/usr/bin/env bash
set -euo pipefail

# macOS does not have realpath and readlink does not have -f option, so do this instead:
script_dir=$( cd "$(dirname "$0")" ; pwd -P )
build_dir="${script_dir}/build"

if [[ "${1:-}" == "--clean" ]]; then
    echo "Cleaning build directory..."
    rm -rf "${build_dir}"
    shift
fi

mkdir -p "${build_dir}"
pushd "${build_dir}"

cmake ..
cmake --build .

if [[ "${1:-}" == "run" ]]; then  # Run the executable after building; pass any remaining arguments to it.
    shift
    ./objectbox-examples-tasks-sync "$@"
fi

popd
