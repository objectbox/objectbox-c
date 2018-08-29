#!/usr/bin/env bash
set -e

ref=objectbox-c/0.1@objectbox/testing
conan test . ${ref}
conan export-pkg -f . ${ref}
conan info .
