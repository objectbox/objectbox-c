#!/usr/bin/env bash
# ObjectBox C test script: download the library, build the test and run it

if [ -z `which nproc` ]; then
    nproc() {
    	sysctl -n hw.ncpu
    }
fi


set -e

case $1 in
--clean)
    clean=true
    shift
    ;;
esac

buildDir=${1:-build-test}

if ${clean:-false} ; then
    echo "Cleaning \"${buildDir}\"..."
    rm -rfv ${buildDir}
    exit 0
fi

./download.sh --quiet

# Don't use any installed library; but the one that we just downloaded
export LD_LIBRARY_PATH=${PWD}/lib

echo "Building into \"${buildDir}\"..."
mkdir -p ${buildDir}
cd ${buildDir}

cmake ..
make -j`nproc`
src-test/objectbox-c-test

echo "Done. All looks good. Welcome to ObjectBox! :)"