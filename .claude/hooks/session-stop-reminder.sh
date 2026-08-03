#!/usr/bin/env bash
# Malwarium Stop hook — non-blocking end-of-session nudge.
# If the tree has uncommitted source/doc changes, remind (once, without blocking the
# stop) to sync docs + commit while context is fresh. Overlaps intentionally with the
# /sync-docs and /handoff skills; this is the automatic backstop. Disable by removing
# the Stop entry in .claude/settings.json.

dir="${CLAUDE_PROJECT_DIR:-$(pwd)}"
changed=$(git -C "$dir" status --porcelain 2>/dev/null | grep -E '\.(cpp|h|md)$' || true)

if [ -n "$changed" ]; then
  n=$(printf '%s\n' "$changed" | grep -c .)
  printf '{"systemMessage": "%s uncommitted source/doc change(s). If this session changed what is built or blocked, consider /sync-docs and a commit before pausing."}\n' "$n"
fi

exit 0
