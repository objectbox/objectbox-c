#!/usr/bin/env bash
set -e

ref=objectbox-c/0.1@objectbox/testing
conan test . ${ref}
conan export-pkg -f . ${ref}
conan info .

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
