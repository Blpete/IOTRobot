#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2018 Barry Peterson
#
# Build IOTRobot with the pinned ESP-IDF image.
# Uses podman when it is installed, otherwise docker.
# Extra arguments are passed to the container instead of the default build,
# for example: ./build.sh idf.py menuconfig

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

VERSION="$(tr -d '[:space:]' < "$ROOT/idf-version.txt")"
IMAGE="${IDF_IMAGE:-docker.io/espressif/idf:${VERSION}}"

if command -v podman >/dev/null 2>&1; then
  RUNNER="podman"
elif command -v docker >/dev/null 2>&1; then
  RUNNER="docker"
else
  echo "Install Podman (https://podman-desktop.io/) or Docker." >&2
  exit 1
fi

export SDKCONFIG_DEFAULTS="sdkconfig.defaults"
if [ -f sdkconfig.defaults.private ]; then
  SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.private"
  if [ -f sdkconfig ] && [ sdkconfig.defaults.private -nt sdkconfig ]; then
    echo "sdkconfig.defaults.private is newer than sdkconfig; removing sdkconfig"
    rm -f sdkconfig
  fi
fi

if [ "$#" -eq 0 ]; then
  set -- idf.py set-target esp32 build
fi

interactive=()
if [ -t 0 ] && [ -t 1 ]; then
  case " $* " in
    *" menuconfig "*|*" monitor "*) interactive=(-it) ;;
  esac
fi

userns=()
if [ "$RUNNER" = "podman" ] && [ "$(uname -s)" = "Linux" ]; then
  userns=(--userns=keep-id)
fi

echo "Using $RUNNER image $IMAGE"
exec "$RUNNER" run --rm \
  "${userns[@]}" \
  "${interactive[@]}" \
  -e "SDKCONFIG_DEFAULTS=${SDKCONFIG_DEFAULTS}" \
  -e CCACHE_DIR=/tmp/ccache \
  -v "$ROOT:/project" \
  -w /project \
  "$IMAGE" \
  "$@"
