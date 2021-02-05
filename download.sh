#!/usr/bin/env bash

# ObjectBox libraries are hosted in a Conan repository on Bintray:
# This script downloads the current version of the library and extracts/installs it locally.
# The download happens in a "download" directory.
# After download and extraction, the script asks if the lib should be installed in /usr/local/lib.
#
# Windows note: to run this script you need to install a bash like "Git Bash".
# Plain MINGW64, Cygwin, etc. might work too, but was not tested.

set -eu

#default values
quiet=false
printHelp=false
libBuildDir="$(pwd)/lib"
###

case ${1:-} in
-h|--help)
    printHelp=true
    ;;
--quiet)
    quiet=true
    shift
    ;;
--install)
    quiet=true
    installLibrary=true
    shift
    ;;
--uninstall)
    uninstallLibrary=true
    shift
    ;;
esac

tty -s || quiet=true

# Note: optional arguments like "--quiet" shifts argument positions in the case block above

version=${1:-0.12.0}
repoType=${2:-testing}
os=${3:-$(uname)}
arch=${4:-$(uname -m)}
echo "Base config: OS ${os} and arch ${arch}"

if [[ "$os" == MINGW* ]] || [[ "$os" == CYGWIN* ]]; then
    echo "Adjusted OS to Windows"
    os=Windows
fi

if [[ "$os" == "Darwin" ]]; then
    echo "Adjusted OS to Macos"
    os=Macos
fi

if [[ $arch == "aarch64" ]]; then
    arch=armv8
    echo "Selected ${arch} architecture for download"
elif [[ $arch == armv7* ]] && [[ $arch != "armv7" ]]; then
    arch=armv7
    echo "Selected ${arch} architecture for download (hard FP only!)"
elif [[ $arch == armv6* ]] && [[ $arch != "armv6" ]]; then
    arch=armv6
    echo "Selected ${arch} architecture for download (hard FP only!)"
fi

conf="${os}::${arch}"
echo "Using configuration ${conf}"

# sudo might not be defined (e.g. when building a docker image)
sudo="sudo"
if [ ! -x "$(command -v sudo)" ]; then
    sudo=""
fi

# original location where we installed in previous versions of this script
oldLibDir=

if [[ "$os" = "Macos" ]]; then
    libFileName=libobjectbox.dylib
    libDirectory=/usr/local/lib
elif [[ "$os" = "Windows" ]]; then
    libFileName=objectbox.dll

    # this doesn't work in Git Bash, fails silently
    # sudo="runas.exe /user:administrator"
    # libDirectory=/c/Windows/System32

    # try to determine library path based on gcc.exe path
    libDirectory=""
    if [ -x "$(command -v gcc)" ] && [ -x "$(command -v dirname)" ] && [ -x "$(command -v realpath)" ]; then
        libDirectory=$(realpath "$(dirname "$(command -v gcc)")/../lib")
    fi
else
    libFileName=libobjectbox.so
    libDirectory=/usr/lib
    oldLibDir=/usr/local/lib
fi


function printUsage() {
    echo "download.sh [\$1:version] [\$2:repo type] [\$3:os] [\$4:arch]"
    echo
    echo "  Options (use at front only):"
    echo "    --quiet: skipping asking to install to ${libDirectory}"
    echo "    --install: install library to ${libDirectory}"
    echo "    --uninstall: uninstall from ${libDirectory}"
}

if ${printHelp} ; then
    printUsage
    exit 0
fi

function uninstallLib() {
    dir=${1}

    if ! [ -f "${dir}/${libFileName}" ] ; then
        echo "${dir}/${libFileName} doesn't exist, can't uninstall"
        exit 1
    fi

    echo "Removing ${dir}/${libFileName}"

    if [ -x "$(command -v ldconfig)" ]; then
        linkerUpdateCmd="ldconfig ${dir}"
    else
        linkerUpdateCmd=""
    fi

    $sudo bash -c "rm -fv '${dir}/${libFileName}' ; ${linkerUpdateCmd}"
}

if ${uninstallLibrary:-false}; then
    uninstallLib "${libDirectory}"

    if [ -x "$(command -v ldconfig)" ]; then
        libInfo=$(ldconfig -p | grep "${libFileName}" || true)
    else
        libInfo=$(ls -lh ${libDirectory}/* | grep "${libFileName}" || true) # globbing would give "no such file" error
    fi

    if [ -z "${libInfo}" ]; then
        echo "Uninstall successful"
    else
        echo "Uninstall failed, leftover files:"
        echo "${libInfo}"
        exit 1
    fi

    exit 0
fi

downloadDir=download

while getopts v:d: opt
do
    case $opt in
        d) downloadDir=$OPTARG;;
        v) version=$OPTARG;;
        *) printUsage
           exit 1 ;;
   esac
done

HASHES="
Linux::x86_64 4db1be536558d833e52e862fd84d64d75c2b3656
Linux::armv6 4a625f0bd5f477eacd9bd35e9c44c834d057524b
Linux::armv7 d42930899c74345edc43f8b7519ec7645c13e4d8
Linux::armv8 b0bab81756b4971d42859e9b1bc6f8b3fa8e036e
Windows::x86 11e6a84a7894f41df553e7c92534c3bf26896802
Windows::x86_64 ca33edce272a279b24f87dc0d4cf5bbdcffbc187
Macos::x86_64 46f53f156846659bf39ad6675fa0ee8156e859fe
" #END_OF_HASHES
hash=$( awk -v key="${conf}" '$1 == key {print $NF}' <<< "$HASHES" )
if [ -z "$hash" ]; then
    echo "Error: the platform configuration ${conf} is unsupported."
    echo "You can select the configuration manually (use --help for details)"
    echo "Possible values are:"
    awk '$0 {print " - " $1 }'  <<< "$HASHES"
    exit 1
fi

baseName=libobjectbox-${version}-${hash}
targetDir="${downloadDir}/${repoType}/${baseName}"
archiveFile="${targetDir}.tgz"
remoteRepo="https://dl.bintray.com/objectbox/conan/objectbox/objectbox-c"
downloadUrl="${remoteRepo}/${version}/${repoType}/0/package/${hash}/0/conan_package.tgz"

echo "Downloading ObjectBox library version ${version} ${repoType} (${hash})..."
mkdir -p "$(dirname "${archiveFile}")"

# Support both curl and wget because their availability is platform dependent
if [ -x "$(command -v curl)" ]; then
    curl --location --fail --output "${archiveFile}" "${downloadUrl}"
else
    wget --no-verbose --output-document="${archiveFile}" "${downloadUrl}"
fi

if [[ ! -s ${archiveFile} ]]; then
    echo "Error: download failed (file ${archiveFile} does not exist or is empty)"
    exit 1
fi

echo "Downloaded:"
du -h "${archiveFile}"

echo
echo "Extracting into ${targetDir}..."
mkdir -p "${targetDir}"
tar -xzf "${archiveFile}" -C "${targetDir}"

if [ ! -d "${libBuildDir}"  ] || ${quiet} ; then
    mkdir -p "${libBuildDir}"
    cp "${targetDir}"/lib/* "${libBuildDir}"
    echo "Copied to ${libBuildDir}:"
    ls -l "${libBuildDir}"
else
    read -p "${libBuildDir} already exists. Copy the just downloaded library to it? [Y/n] " -r
    if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z "$REPLY" ]] ; then
        cp "${targetDir}"/lib/* "${libBuildDir}"
        echo "Copied; contents of ${libBuildDir}:"
        ls -l "${libBuildDir}"
    fi
fi

if ${quiet} ; then
    if ! ${installLibrary:-false}; then
        echo "Skipping installation to ${libDirectory} in quiet mode"
        if [ -f "${libDirectory}/${libFileName}" ]; then
            echo "However, you have a library there:"
            ls -l "${libDirectory}/${libFileName}"
        fi
    fi
else
    if [ -z "${installLibrary:-}" ] && [ -n "${libDirectory}" ]; then
        read -p "OK. Do you want to install the library into ${libDirectory}? [Y/n] " -r
        if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z "$REPLY" ]] ; then
            installLibrary=true

            if [ -n "${oldLibDir}" ] && [ -f "${oldLibDir}/${libFileName}" ] ; then
                echo "Found an old installation in ${oldLibDir} but a new one is going to be placed in ${libDirectory}."
                echo "It's recommended to uninstall the old library to avoid problems."
                read -p "Uninstall from old location? [Y/n] " -r
                if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z "$REPLY" ]] ; then
                    uninstallLib ${oldLibDir}
                fi
            fi

        fi
    fi
fi

if ${installLibrary:-false}; then
    echo "Installing ${libDirectory}/${libFileName}"
    $sudo cp "${targetDir}/lib/${libFileName}" ${libDirectory}

    if [ -x "$(command -v ldconfig)" ]; then
        $sudo ldconfig "${libDirectory}"
        libInfo=$(ldconfig -p | grep "${libFileName}" || true)
    else
        libInfo=$(ls -lh ${libDirectory}/* | grep "${libFileName}" || true) # globbing would give "no such file" error
    fi

    if [ -z "${libInfo}" ]; then
        echo "Error installing the library - not found"
        exit 1
    else
        echo "Installed objectbox libraries:"
        echo "${libInfo}"
    fi
fi