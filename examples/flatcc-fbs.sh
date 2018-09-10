#!/usr/bin/env bash
set -e

flatcc --version

cd tasks/
flatcc --common --builder tasks.fbs
cd ..