#!/bin/bash
# session-start.sh — make a fresh Claude Code on the web container able to build and
# test this repo the moment the session opens.
#
# A remote session starts from a clean clone with no Python packages, so
# `./tools/gates.sh` dies on its very first step — gen_item_icons needs Pillow —
# before it has compiled a line. Everything below is what the tools/ scripts import,
# plus the codegen the engine does not exist without.
#
# THE S3 TIER CANNOT RUN HERE, and no hook can fix that: the sandbox network policy
# refuses api.registry.platformio.org, so PlatformIO can never fetch the espressif32
# toolchain. Run `./tools/gates.sh --native` in a remote session and let CI build the
# firmware — .github/workflows/gates.yml runs both tiers on every push, including the
# claude/** branches these sessions work on.
set -euo pipefail

# A local machine has its own toolchain and its own opinions about what is installed
# into it. Only the containers start bare, and only they are this script's business.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-$(dirname "$0")/../..}"

# Pillow for the sprite/icon generators; PyYAML for the workflow-parse gate, which
# SKIPS ITSELF when PyYAML is missing — so without this the one gate that catches an
# unparseable workflow passes by never running, which is the failure it exists to stop.
pip install --quiet --disable-pip-version-check pillow pyyaml

# src/generated/ is compiled from assets/ and is not committed, so a clean clone has no
# engine to compile until this has run.
python3 tools/gen_assets.py
