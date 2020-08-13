#!/usr/bin/env bash
set -euo pipefail

flatcc=flatcc
obxgen=objectbox-generator

${flatcc} --version
${obxgen} -version

(
  cd c-cursor-no-gen
  ${flatcc} --common --builder task.fbs
)

(
  cd c-gen
  ${obxgen} -c tasklist.fbs
)

(
  cd cpp-gen
  ${obxgen} -cpp tasklist.fbs
)