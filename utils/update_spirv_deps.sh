#!/bin/bash
if [ ! -d external -o ! -d .git -o ! -d azure-pipelines ] ; then
  echo "Run this script on the top-level directory of the DXC repository."
  exit 1
fi

set -ex
git submodule foreach 'set -x && git switch main && git pull --ff-only'
git add external
exit 0
