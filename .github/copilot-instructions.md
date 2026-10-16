## Contribution requirements

Before making changes or opening a pull request, follow the "Before
contributing (read first)" section of [AGENTS.md](../AGENTS.md). It covers the
required development environment (including TAEF), required validation, test
input rules, AI-use disclosure, and the requirement that a human review every
change before a pull request is opened.

## Pull request policy review guidance

When reviewing pull requests, check them against [AIToolPolicy.md](../AIToolPolicy.md) and the testing
requirements in `CONTRIBUTING.md`. Raise these issues as review comments:
- The description says that tests were skipped or could not be run, for example
  because TAEF, `te.exe`, a tool, or a platform was unavailable. Also flag
  descriptions that report only a subset of lit suites for a functional change.
  Point out that the full suite, including TAEF, must be run locally (see
  "Required development environment" in `AGENTS.md`).
- The change contains substantial tool-generated content but has no
  disclosure, such as an `Assisted-by:` trailer or a note in the description.
  Only flag this when there are clear signs of tool generation.
- The change adds opaque binary test inputs, such as fuzzed or bit-flipped
  bitcode, instead of human-readable `.hlsl` or textual `.ll` inputs.
- A functional change adds no test that would fail without the fix.

Keep these comments factual and courteous, and link to the relevant policy
section.

## Pull request review guidance

When reviewing pull requests in this repository, always check whether `docs/ReleaseNotes.md` should be updated.

Use the release note policy in `CONTRIBUTING.md` ("Release Notes") as the default:
- Release notes are expected for user-visible, significant compiler behavior changes.
- Common examples include new features (language, hardware support, compiler options), important isolated bug fixes, and changes in default behavior.
- Release notes are often not needed for refactors, test-only updates, or infrastructure-only changes unless user-visible behavior changes.

Account for multi-PR efforts:
- If a PR appears to be one part of a larger tracked effort, recognize that a single shared release note may be intentional.
- In those cases, prefer confirming that release note coverage exists (or is planned) across the broader effort, rather than requiring duplicate entries per PR.

How to comment when release notes are missing:
- If a release note is clearly warranted and missing, call it out and point to `docs/ReleaseNotes.md`.
- If it is not obvious whether one is required, leave only a gentle prompt: **"Did you consider adding a release note?"**

When to skip a release note comment:
- If the PR already updates `docs/ReleaseNotes.md`, review the entry's placement and content instead of asking for an additional release note.
- If the PR is docs-only and does not modify `docs/ReleaseNotes.md`, do not leave a release note comment.
- If the PR is a dependency bump (for example, a "Bump ..." PR), do not leave a release note comment.

When reviewing an existing release note:
- Check it against the release note policy and entry rules in `CONTRIBUTING.md`.
- Review the PR as merged with the current base branch, not only the head branch's file contents. If the head branch is behind or diverged from the base branch, account for newer release headings already on the base branch and flag the PR for an update when the stale branch makes the entry's final placement ambiguous or incorrect.
- Put changes targeting the next release under `### Upcoming Release`.
- Use `### Upcoming Preview Release` only for changes that apply exclusively to experimental preview shader models.
- Do not add entries to an already named release unless the change explicitly targets that release.

Comment tone:
- Use a stronger ask when the PR clearly appears to be a user-visible bug fix or feature.
- If release-note coverage may come in a related PR (including a future PR), ask the author to point to that planned coverage.
- If uncertain, prefer the gentle **"Did you consider adding a release note?"** wording.
