#!/usr/bin/env bash
set -euo pipefail

# macOS does not have realpath and readlink does not have -f option, so do this instead:
script_dir=$( cd "$(dirname "$0")" ; pwd -P )

# Adjust to your local paths if you don't have flatcc/objectbox-generator in your system path
flatcc=flatcc
#flatcc="${script_dir}/../../flatcc/bin/flatcc"  # from checked out repo
obxgen=objectbox-generator
#obxgen="${script_dir}/../../objectbox-generator/objectbox-generator"  # from checked out repo

${flatcc} --version || true
${obxgen} -version

(
  cd c-cursor-no-gen
  ${flatcc} --common --builder task.fbs || echo "  >> Warning: without flatcc, you cannot generate plain C sources <<"
)

(
  cd c-gen
  ${obxgen} -c tasklist.fbs
)

(
  cd cpp-gen
  ${obxgen} -cpp11 tasklist.fbs
)

(
  cd cpp-gen-sync
  ${obxgen} -cpp11 tasklist.fbs
)

(
  cd vectorsearch-cities
  ${obxgen} -cpp city.fbs
)
