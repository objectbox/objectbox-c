#!/usr/bin/env bash
set -e

version="0.2"
ref=objectbox-c/${version}@objectbox/testing
existingID=$(conan search ${ref} | grep "Package_ID:" | awk '{print $NF}')
if [ -z "$existingID" ]; then
    echo "$ref does not yet exist. Exporting..."
    conan export-pkg . ${ref}
else
    echo "$ref already exist with package ID $existingID. Forcing export..."
    conan export-pkg -f . ${ref}
fi

conan test . ${ref}

newID=$(conan info . | grep " ID:" | awk '{print $NF}')
if [ -z "$newID" ]; then
    echo "Could not grep ID from conan info. Output:"
    conan info .
    exit 1
fi

hashesFile=download.sh
hashEntry=$(grep "$newID" ${hashesFile}) || true
if [ -z "$hashEntry" ]; then
    arch=$(uname -m)
    os=$(uname)
    hashEntry="# ${os}::${arch} ${newID}"
    printf "\n$hashEntry" >> ${hashesFile}
    echo "Added hash entry ($hashEntry) to ${hashesFile}"
else
    echo "Hash entry ($hashEntry) already present in ${hashesFile}"
fi

read -p "Upload to bintray? [y/N] " -r
if [[ $REPLY =~ ^[Yy]$ ]]; then
    obxRemote=$(conan remote list | grep obx-bintray) || true
    echo "Using Conan remote: $obxRemote"
    if [ -z "$obxRemote" ]; then
        echo "The Conan remote \"obx-bintray\" was not set up set. Run this:"
        echo "conan remote add obx-bintray https://api.bintray.com/conan/objectbox/conan"
        echo "conan user -p \$password -r obx-bintray \$user"
        exit 1
    fi
    conan upload ${ref} --all -r=obx-bintray
fi
