#!/usr/bin/env bash

# ObjectBox libraries are hosted in a Conan repository on Bintray:
# This script downloads the current version of the library and extracts/installs it locally.
# The download happens in a "download" directory.
# After download and extraction, the script asks if the lib should be installed in /usr/local/lib.
#
# Windows note: to run this script you need to install a bash like "Git Bash".
# Plain MINGW64, Cygwin, etc. might work too, but was not tested.

set -e

#default value
quiet=false

case $1 in
-h|--help)
    echo "download.sh [\$1:version] [\$2:repo type] [\$3:os] [\$4:arch]"
    echo
    echo "  Options (use at front only):"
    echo "    --quiet: skipping asking to install to /usr/local/lib"
    echo "    --uninstall: uninstall from /usr/local/lib"
    exit 0
    ;;
--quiet)
    quiet=true
    shift
    ;;
--uninstall)
    sudo rm -v /usr/local/lib/libobjectbox.so
    sudo ldconfig /usr/local/lib
    echo "Uninstalled objectbox libraries; verifying (no more output expected after this line)"
    ldconfig -p | grep objectbox
    exit 0
    ;;
esac

# allow passing version as a second argument
version=${1:-0.3}

# repo as a third argument
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

conf=${os}::${arch}
echo "Using configuration ${conf}"

downloadDir=download

while getopts v:d: opt
do
   case $opt in
       d) downloadDir=$OPTARG;;
       v) version=$OPTARG;;
   esac
done

hash=$(grep "# conan-conf ${conf}" $0 | awk '{print $NF}')
if [ -z "$hash" ]; then
    echo "Error: the platform configuration ${conf} is unsupported."
    echo "You can select the configuration manually (use --help for details)"
    echo "Possible values are: "
    awk '/^# conan-conf/ {print $3}' $0
    exit 1
fi

baseName=libobjectbox-${version}-${hash}
targetDir=${downloadDir}/${repoType}/${baseName}
archiveFile=${targetDir}.tgz
remoteRepo="https://dl.bintray.com/objectbox/conan/objectbox/objectbox-c"
downloadUrl="${remoteRepo}/${version}/${repoType}/package/${hash}/conan_package.tgz"

echo "Downloading ObjectBox library version ${version} ${repoType} (${hash})..."
mkdir -p $(dirname ${archiveFile})

# Support both curl and wget because their availability is platform dependent
if [ -x "$(command -v curl)" ]; then
    curl -L -o "${archiveFile}" "${downloadUrl}"
else
    #wget too verbose with redirects, pipe and grep only errors
    wget -O "${archiveFile}" "${downloadUrl}" 2>&1 | grep -i "HTTP request sent\|failed\|error"
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

if [[ ${os} == Windows ]]; then
    echo "OK. The ObjectBox dll is available here:"
    dllFullPath=$(realpath ${targetDir}/lib/objectbox-c.dll)
    echo "${dllFullPath}"
    echo "And with backslashes:"
    echo "${dllFullPath}" | tr '/' '\\'
    exit 0 # Done, the remainder of the script is non-Windows
fi

if [[ ! -d "lib"  || ${quiet} ]]; then
    mkdir -p lib
    cp ${targetDir}/lib/* lib/
    echo "Copied to local lib directory:"
    ls -l lib/
else
    read -p "Local lib directory already exists. Copy the just downloaded library to it? [Y/n] " -r
    if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z "$REPLY" ]] ; then
        cp ${targetDir}/lib/* lib/
        echo "Copied; contents of the local lib directory:"
        ls -l lib/
    fi
fi

if ${quiet} ; then
    echo "Skipping installation to /usr/local/lib in quiet mode"
    exit 0
fi

read -p "OK. Do you want to install the library into /usr/local/lib? [y/N] " -r
if [[ $REPLY =~ ^[Yy]$ ]]; then
    # TODO sudo is not be available on all platforms - provide an alternative
    sudo cp ${targetDir}/lib/* /usr/local/lib
    sudo ldconfig /usr/local/lib
    echo "Installed objectbox libraries:"
    ldconfig -p | grep objectbox
fi

# Known Conan hashes; this script will grep for those
# conan-conf Linux::x86_64 4db1be536558d833e52e862fd84d64d75c2b3656
# conan-conf Linux::armv6 4a625f0bd5f477eacd9bd35e9c44c834d057524b
# conan-conf Linux::armv7 d42930899c74345edc43f8b7519ec7645c13e4d8
# conan-conf Windows::x86_64 ca33edce272a279b24f87dc0d4cf5bbdcffbc187
# conan-conf Macos::x86_64 46f53f156846659bf39ad6675fa0ee8156e859fe
