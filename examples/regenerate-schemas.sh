#!/usr/bin/env bash
set -euo pipefail

# Adjust to your local paths if you don't have it in your system path
flatcc=flatcc
obxgen=objectbox-generator

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
  ${obxgen} -cpp tasklist.fbs
)