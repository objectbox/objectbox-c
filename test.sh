#!/usr/bin/env bash
# ObjectBox C test script: build the test and run it

set -eo pipefail

case $1 in
--clean)
  clean=true
  shift
  ;;
esac

buildDir=${1:-build-test}

if ${clean:-false}; then
  echo "Cleaning \"${buildDir}\"..."
  rm -rfv ${buildDir}
  exit 0
fi

buildSubDir=
if [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
  buildSubDir=Debug
  # After `cmake ..` is executed, we need to copy the .dll to the same dir as the test executable.
  # This path may change in future CMake versions so watch for `cp` failures after upgrading CMake.
  # Since test.sh is here (almost exclusively) for CI, end-users of the library aren't affected if the path changes.
  testPrepCmd="cp ../../_deps/objectbox-download-src/lib/objectbox.dll ./"
fi

# Don't use any installed library; but the one downloaded by CMake
export LD_LIBRARY_PATH=

echo "Building into \"${buildDir}\"..."
mkdir -p ${buildDir}
cd ${buildDir}
cmake ..
cmake --build . --target objectbox-c-test
cmake --build . --target objectbox-c-gen-test
cmake --build . --target objectbox-c-examples-tasks-c-gen
cmake --build . --target objectbox-c-examples-tasks-cpp-gen
cmake --build . --target objectbox-c-examples-tasks-cpp-gen-sync
(cd src-test/${buildSubDir} && ${testPrepCmd} && ./objectbox-c-test)
(cd src-test-gen/${buildSubDir} && ${testPrepCmd} && ./objectbox-c-gen-test)

echo "Done. All looks good. Welcome to ObjectBox! :)"
