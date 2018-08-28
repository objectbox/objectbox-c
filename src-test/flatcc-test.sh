#!/usr/bin/env bash
set -e

flatccCmd=./../../../../../../../flatcc/bin/flatcc
${flatccCmd} --version
${flatccCmd} --common --builder c_test.fbs
