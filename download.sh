#!/usr/bin/env bash

# ObjectBox libraries are hosted in a Conan repository on Bintray:
# This script downloads the current version of the library and extracts/installs it locally.
# The download happens in a "download" directory.
# After download and extraction, the script asks if the lib should be installed in /usr/local/lib.
#
# Windows note: to run this script you need to install a bash like "Git Bash".
# Plain MINGW64, Cygwin, etc. might work too, but was not tested.

set -e

arch=$(uname -m)
os=$(uname)
echo "Detected ${os} running on ${arch}"

downloadDir=download
version=0.1

while getopts v:d: opt
do
   case $opt in
       d) downloadDir=$OPTARG;;
       v) version=$OPTARG;;
   esac
done

key=${os}::${arch}::${version}
hash=$(grep "${key}" $0 | awk '{print $NF}')
if [ -z "$hash" ]; then
    echo "Error: the configuration ${key} is unsupported (no Conan hash registered)."
    echo "Please update this script. If you already did, it seems this configuration is not yet supported."
    exit 1
fi

baseName=libobjectbox-${version}-${hash}
archiveFile=${downloadDir}/${baseName}.tgz
targetDir=${downloadDir}/${baseName}
remoteRepo="https://dl.bintray.com/objectbox/conan/objectbox/objectbox-c"
downloadUrl="${remoteRepo}/${version}/testing/package/${hash}/conan_package.tgz"

echo "Downloading ObjectBox library version ${version} (${hash})..."
mkdir -p "${downloadDir}"

# Support both curl and wget because their availability is platform dependent
if [ -x "$(command -v curl)" ]; then
    curl -o "${archiveFile}" "${downloadUrl}"
else
    #wget too verbose with redirects, pipe and grep only errors
    wget -O "${archiveFile}" "${downloadUrl}" 2>&1 | grep -i "HTTP request sent\|failed\|error"
fi

if [[ ! -s ${archiveFile} ]]; then
    echo "Error: download failed"
    exit 1
fi

echo "Downloaded:"
du -h "${archiveFile}"

echo
echo "Extracting into ${targetDir}..."
mkdir -p "${targetDir}"
tar -xzf "${archiveFile}" -C "${targetDir}"

if [[ ${os} == MINGW* ]] || [[ ${os} == CYGWIN* ]]; then
    echo "OK. The ObjectBox dll is available here:"
    dllFullPath=$(realpath ${targetDir}/lib/objectbox-c.dll)
    echo "${dllFullPath}"
    echo "And with backslashes:"
    echo "${dllFullPath}" | tr '/' '\\'
    exit 0 # Done, the remainder of the script is non-Windows
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
# Linux::x86_64::0.1 4db1be536558d833e52e862fd84d64d75c2b3656
# MINGW64_NT-10.0::x86_64::0.1 ca33edce272a279b24f87dc0d4cf5bbdcffbc187