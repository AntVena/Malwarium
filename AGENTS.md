# AGENTS.md

Before changing anything in this repo, read [`CONTRIBUTING.md`](CONTRIBUTING.md) and the
standards it links. They are the conventions this codebase is written and reviewed against;
this file only points at them.

## Finishing work

Finished work ships without asking the owner first. Follow *Releasing* in CONTRIBUTING.md:
when the gates pass, bump the versions, record the golden walks again and commit the
release as `release vX.Y.Z: <what>`. A Claude session pushes that commit to its own
`claude/**` or `ccr-*` branch; `.github/workflows/release-merge.yml` fast-forwards `main` to
it once CI's gates pass there, and dispatches `publish.yml`. If that run reports `main` has
moved, merge `main` into the branch, run the gates and push a new release commit.

Ask first only for the two changes ORIENTATION.md's *Releasing* names: a save change that
breaks the append-only stream, and a partition-table or bootloader change.
