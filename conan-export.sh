#!/usr/bin/env bash
set -e

cd `dirname "$0"`

# default value
forceAlways=false
upload=false
quiet=false

while test $# -gt 0; do
case "$1" in
-h|--help)
    echo "conan-export.sh [\$1:version] [\$2:repo type] [\$3:os] [\$4:arch]"
    echo
    echo "  Use env variable OBX_CMAKE_TOOLCHAIN to export for a different toolchain (cross compilation):"
    echo "    possible values: arm-linux-gnueabihf, arm-linux-gnueabi, armv6hf"
    echo
    echo "  Options (use at front only):"
    echo "    --force: always force the export; e.g. if only the recipe exists"
    echo "             (e.g. if you get ERROR: Package already exists)"
    exit 0
    ;;
--force)
    forceAlways=true
    shift
    ;;
--upload)
    upload=true
    shift
    ;;
--quiet)
    quiet=true
    shift
    ;; 
*)
    break
    ;;
esac
done

# allow passing version and channel as arguments (for CI/testing)
version=${1:-0.6}
repoType=${2:-testing}
os=${3:-`uname`}
arch=${4:-`uname -m`}
echo "Base config: OS ${os} and arch ${arch}"

if [[ ${os} == MINGW* ]] || [[ ${os} == CYGWIN* ]]; then
    echo "Adjusted OS to Windows"
    os=Windows
fi

if [[ ${os} == "Darwin" ]]; then
    echo "Adjusted OS to Macos"
    os=Macos
fi

if [[ $arch == armv7* ]] && [[ $arch != "armv7" ]]; then
    arch=armv7
    echo "Selected ${arch} architecture for download (hard FP only!)"
fi

if [[ $arch == armv6* ]] && [[ $arch != "armv6" ]]; then
    arch=armv6
    echo "Selected ${arch} architecture for download (hard FP only!)"
fi

ref=objectbox-c/${version}@objectbox/${repoType}
existingID=$(conan search ${ref} | grep "Package_ID:" | awk '{print $NF}')
exportArgs=
if ${forceAlways} ; then
    exportArgs="$exportArgs -f"
elif [ -z "$existingID" ]; then
    echo "$ref does not yet exist. Exporting..."
else
    echo "$ref already exist with package ID $existingID. Forcing export..."
    exportArgs="$exportArgs -f"
fi

if [[ -n "${OBX_CMAKE_TOOLCHAIN}" ]]; then
    echo "Env var OBX_CMAKE_TOOLCHAIN is set to \"${OBX_CMAKE_TOOLCHAIN}\""

    if [ "${OBX_CMAKE_TOOLCHAIN}" == "arm-linux-gnueabihf" ]; then
        os=Linux
        arch=armv7 # Note that RPi 3 identifies as armv7 within Conan (not as armv7hf although it has HF support)
    elif [ "${OBX_CMAKE_TOOLCHAIN}" == "arm-linux-gnueabi" ]; then
        os=Linux
        arch=armv5 # NOTE armv6 is the lowest available for conan
    elif [ "${OBX_CMAKE_TOOLCHAIN}" == "armv6hf" ]; then
        os=Linux
        arch=armv6
    else
        echo "Value for OBX_CMAKE_TOOLCHAIN was not recognized"
        exit 1
    fi

    echo "Overrides set by OBX_CMAKE_TOOLCHAIN: os=${os}, arch=${arch}"
fi

if [[ -n "$os" ]]; then
    commonArgs="$commonArgs -s os=${os}"
fi

if [[ -n "$arch" ]]; then
    commonArgs="$commonArgs -s arch=${arch}"
fi

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
    hashEntry="${os}::${arch} ${hash}"
    sed -i "/END_OF_HASHES/i \
${hashEntry}" ${hashesFile}
    echo "Added hash entry ($hashEntry) to ${hashesFile}"
else
    echo "Hash entry ($hashEntry) already present in ${hashesFile}"
fi

localPackageLibDir="${HOME}/.conan/data/objectbox-c/${version}/objectbox/${repoType}/package/${hash}/lib"
echo "Local Conan lib dir: ${localPackageLibDir}"
ls -lh "${localPackageLibDir}"

if ! ${quiet} ; then
    if [ -z "${OBX_CMAKE_TOOLCHAIN}" ]; then
        read -p "OK. Do you want to install the library into /usr/local/lib? [y/N] " -r
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            # TODO sudo is not be available on all platforms - provide an alternative

            sudo cp ${localPackageLibDir}/* /usr/local/lib
            if [[ ${os} = "Macos" ]]; then
                # No ldconfig on le mac; /usr/local/lib seemed to work fine though without further work. See also:
                # https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/UsingDynamicLibraries.html
                echo "Installed objectbox libraries:"
                ls -lh /usr/local/lib/*objectbox*
            else
                sudo ldconfig /usr/local/lib
                echo "Installed objectbox libraries:"
                ldconfig -p | grep objectbox
            fi
        fi
    fi
fi

if ! ${upload} ; then
    read -p "Upload to bintray? [y/N] " -r
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        upload=true
    fi
fi

if ${upload} ; then
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
