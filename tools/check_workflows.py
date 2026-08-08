#!/usr/bin/env python3
"""Fail if a GitHub Actions workflow file does not parse as YAML.

Why this is a gate and not a thing you notice: an unparseable workflow does not
report a syntax error. GitHub creates a run, gives it the FILE PATH as its name
instead of the `name:` field, and completes it in zero seconds with no jobs, no
steps and no logs — `gh run view --log-failed` answers "log not found". On a
repo whose releases fire a second workflow that DOES parse, the green publish
sits directly above the broken gates run in `gh run list`, and the whole CI tier
can stay silently dead for several releases.

The trigger for writing this: `- name: 'Pedia data freshness` — a step name whose
leading apostrophe is part of the product's own word ('Pedia). Bare, it opens a
YAML single-quoted scalar that never closes, and the file stops parsing from
there. Every apostrophe-leading name in this repo is a candidate for the same
slip, and the local gates could not see it because tools/gates.sh runs the build,
not the workflow that runs the build.

Checks each file under .github/workflows/ parses, and that what it parses into
still looks like a workflow — `on` and `jobs` present, every job carrying steps.
A file that parses into something structurally wrong is the same outage.
"""
import pathlib
import sys

try:
    import yaml
except ImportError:                      # pragma: no cover - environment guard
    print("check_workflows: PyYAML not installed — skipping", file=sys.stderr)
    sys.exit(0)

ROOT = pathlib.Path(__file__).resolve().parent.parent
WORKFLOWS = ROOT / ".github" / "workflows"


def main() -> int:
    files = sorted(WORKFLOWS.glob("*.yml")) + sorted(WORKFLOWS.glob("*.yaml"))
    if not files:
        print(f"check_workflows: no workflows under {WORKFLOWS}", file=sys.stderr)
        return 1

    failures = []
    for f in files:
        rel = f.relative_to(ROOT)
        try:
            doc = yaml.safe_load(f.read_text())
        except yaml.YAMLError as e:
            failures.append(f"{rel}: does not parse as YAML\n{e}")
            continue
        if not isinstance(doc, dict):
            failures.append(f"{rel}: parses, but not into a mapping")
            continue
        # `on:` is YAML 1.1's boolean true, which safe_load hands back as the KEY
        # True rather than the string "on" — both spellings count as present.
        if "on" not in doc and True not in doc:
            failures.append(f"{rel}: no `on:` trigger block")
        jobs = doc.get("jobs")
        if not isinstance(jobs, dict) or not jobs:
            failures.append(f"{rel}: no `jobs:` block")
            continue
        for name, job in jobs.items():
            if not isinstance(job, dict):
                failures.append(f"{rel}: job `{name}` is not a mapping")
            elif "steps" not in job and "uses" not in job:
                failures.append(f"{rel}: job `{name}` has neither steps nor uses")

    if failures:
        print("Workflow files that would fail to start on GitHub:\n", file=sys.stderr)
        for msg in failures:
            print(f"  {msg}\n", file=sys.stderr)
        return 1
    print(f"check_workflows: {len(files)} workflow(s) parse")
    return 0


if __name__ == "__main__":
    sys.exit(main())
