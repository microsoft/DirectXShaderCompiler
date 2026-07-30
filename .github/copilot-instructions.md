## Pull request review guidance

When reviewing pull requests in this repository, always check whether `docs/ReleaseNotes.md` should be updated.

Use the release note policy in `CONTRIBUTING.md` ("Release Notes") as the default:
- Release notes are expected for user-visible, significant compiler behavior changes, including isolated bug fixes and new features.
- Release notes are often not needed for refactors, test-only updates, or infrastructure-only changes unless user-visible behavior changes.

Account for multi-PR efforts:
- If a PR appears to be one part of a larger tracked effort, recognize that a single shared release note may be intentional.
- In those cases, prefer confirming that release note coverage exists (or is planned) across the broader effort, rather than requiring duplicate entries per PR.

How to comment when release notes are missing:
- If a release note is clearly warranted and missing, call it out and point to `docs/ReleaseNotes.md`.
- If it is not obvious whether one is required, leave only a gentle prompt: **"Did you consider adding a releasenote?"**
