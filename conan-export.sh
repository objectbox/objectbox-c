#!/usr/bin/env bash
set -e

# allow passing version and channel as arguments (for CI/testing)
version=${1:-0.3}
repoType=${2:-testing}

ref=objectbox-c/${version}@objectbox/${repoType}
existingID=$(conan search ${ref} | grep "Package_ID:" | awk '{print $NF}')
exportArgs=
if [ -z "$existingID" ]; then
    echo "$ref does not yet exist. Exporting..."
else
    echo "$ref already exist with package ID $existingID. Forcing export..."
    exportArgs="$exportArgs -f"
fi

if [ "${OBX_CMAKE_TOOLCHAIN}" == "arm-linux-gnueabihf" ]; then
    os=Linux
    arch=armv7 # NOTE this is probably wrong and should be armv7hf
elif [ "${OBX_CMAKE_TOOLCHAIN}" == "arm-linux-gnueabi" ]; then
    os=Linux
    arch=armv5 # NOTE armv6 is the lowest available for conan
else
    arch=$(uname -m)
    os=$(uname)
fi
commonArgs="$commonArgs -s os=${os} -s arch=${arch}"

conan export-pkg $exportArgs $commonArgs . ${ref}

conan test $commonArgs . ${ref}

hash=$(conan info $commonArgs . | grep " ID:" | awk '{print $NF}')
if [ -z "$hash" ]; then
    echo "Could not grep ID from conan info. Output:"
    conan info $commonArgs .
    exit 1
fi

hashesFile=download.sh
hashEntry=$(grep "$hash" ${hashesFile}) || true
if [ -z "$hashEntry" ]; then
    hashEntry="# conan-conf ${os}::${arch} ${hash}"
    printf "\n$hashEntry" >> ${hashesFile}
    echo "Added hash entry ($hashEntry) to ${hashesFile}"
else
    echo "Hash entry ($hashEntry) already present in ${hashesFile}"
fi

localPackageLibDir="${HOME}/.conan/data/objectbox-c/${version}/objectbox/${repoType}/package/${hash}/lib"
echo "Local Conan lib dir: ${localPackageLibDir}"
ls -lh "${localPackageLibDir}"

if [ -z "${OBX_CMAKE_TOOLCHAIN}" ]; then
    read -p "OK. Do you want to install the library into /usr/local/lib? [y/N] " -r
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        # TODO sudo is not be available on all platforms - provide an alternative

        sudo cp ${localPackageLibDir}/* /usr/local/lib
        sudo ldconfig /usr/local/lib
        echo "Installed objectbox libraries:"
        ldconfig -p | grep objectbox
    fi
fi

read -p "Upload to bintray? [y/N] " -r
if [[ $REPLY =~ ^[Yy]$ ]]; then
    obxRemote=$(conan remote list | grep obx-bintray) || true
    echo "Using Conan remote: $obxRemote"
    if [ -z "$obxRemote" ]; then
        echo "The Conan remote \"obx-bintray\" was not set up. Run this:"
        echo "conan remote add obx-bintray https://api.bintray.com/conan/objectbox/conan"
        echo "conan user -p \$password -r obx-bintray \$user"
        exit 1
    fi
    conan upload ${ref} --all -r=obx-bintray
fi
