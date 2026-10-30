# DXC agent guidance

## Before contributing (read first)

These requirements come from [AIToolPolicy.md](AIToolPolicy.md) and
[CONTRIBUTING.md](CONTRIBUTING.md). Reviewers close, or request changes on,
pull requests that do not meet them. If you cannot meet a requirement, stop
and tell the human you are working with. Do not work around it, and do not
open a pull request.

### Human in the loop

- The human contributor is the author and must be able to explain every line
  of the change during review. Recommend small, focused changes, especially
  for new contributors.
- Do not open a pull request (for example with `gh pr create`) until the human
  has explicitly confirmed that they have read and reviewed every change, that
  they understand it, and that the testing below was completed. Open pull
  requests from a fork, not from a branch in this repository.
- The human must respond to review feedback personally. Do not post
  auto-generated replies such as "Fixed." on the human's behalf.

### Before starting work

- Check that the issue is not assigned to someone and that no maintainer is
  already working on it. Look at recent comments, linked pull requests, and
  related open pull requests. Maintainers may have unposted fixes for recently
  filed issues. Older issues are safer choices.
- Comment on the issue before starting. Discuss new features and substantial
  changes with the team before writing code.

### Development environment and testing

Contributors can work on any platform DXC supports. Testing on every platform
is not required before opening a pull request.

- Set up a working development environment for your platform, as described in
  [docs/BuildingAndTestingDXC.rst](docs/BuildingAndTestingDXC.rst), including
  initialized submodules (`git submodule update --init --recursive`).
- Build DXC and run the full test suite available on your platform (for
  example, the `check-all` target, or `hcttest` on Windows).
- The contributor must have built and tested every piece of code that the
  change adds or modifies. If the change touches platform-specific code (for
  example, Windows-only TAEF tests), it must have been built and run on that
  platform. If the human cannot test some of the changed code, stop and tell
  them. Do not open a pull request that says changed code is untested.
- Do not work around a broken environment, for example by stubbing out missing
  tools or test substitutions.
- Every functional change needs new tests. For bug fixes, at least one new
  test must fail without the fix. Confirm this.
- Test inputs must be human-readable, such as `.hlsl` or textual `.ll` files.
  Do not add opaque binary test inputs, such as fuzzed or bit-flipped bitcode.
- Format the changed lines with `git-clang-format`.

### Quality of the fix

- Fix the root cause rather than the symptom. Before patching the place where
  a failure shows up, check whether the same problem has been fixed elsewhere,
  and look for a more general fix. See "Investigating compile-time regressions
  and miscompiles" below.
- Follow the release-note rules in `CONTRIBUTING.md`: user-visible changes go
  under `### Upcoming Release` in `docs/ReleaseNotes.md`. Each entry is a
  single sentence with no links to pull requests.

### Pull request contents

- Follow the title and description format in `CONTRIBUTING.md`.
- Disclose tool use in the pull request description and in the commit
  messages, for example with an `Assisted-by: <tool>` trailer.

## Investigating compile-time regressions and miscompiles

- Start from evidence. Identify the exact pass, hot function, and triggering
  operation before proposing a fix. Enumerate every limit, cutoff, cache, and
  heuristic on that path.
- Check this fork before assuming an LLVM design flaw. DXC is based on LLVM
  3.7, and local divergences are commonly, but not always, marked
  `HLSL Change`. Reconstruct the local pre-change behavior before reaching
  outside the repository.
- Inspect local history before designing a new mechanism. Run `git blame` on
  the hot line and use `git log -p -- <file>` and
  `git log -S<symbol> -- <file>` to find when and why it changed. Treat a
  surprising tuning constant as a regression candidate first. Use
  `git show <commit>^:<path>` to inspect the pre-change file. If a pristine
  upstream comparison is still needed, consult the `llvmorg-3.7.1` tag in
  `llvm/llvm-project`.
- Check the scope of shared analysis settings. A file-scope limit changed for
  one pass can affect every consumer of that analysis. Enumerate the users and
  state the affected passes in the PR.
- Prefer restoring an upstream default over adding a DXC-specific limit or
  heuristic to core LLVM code. Require stronger evidence for a new mechanism
  because this fork has limited regression coverage for LLVM analyses and
  transforms.
- Account for the test gap. `test/Transforms/lit.local.cfg` and
  `test/Analysis/lit.local.cfg` disable discovery with
  `config.suffixes = []`, including the ordinary GVN and
  MemoryDependenceAnalysis suites. Add focused coverage under
  `tools/clang/test/DXC/Passes/`, but do not mistake a targeted test for broad
  pass-regression coverage.
- Build reproducers from the original failure's structural characteristics,
  not from the proposed fix. Verify that the repro has the same sensitivity to
  relevant limits and options before trusting it.
- Treat surprising experimental results as new evidence. If an unrelated knob
  outperforms the proposed fix, stop and re-derive the root cause instead of
  continuing the existing plan.
- Use cross-fork comparisons for "how does upstream work now?" and local
  history for "what did DXC change?" Do the cheaper local-history check first.

### Example

`MemoryDependenceAnalysis` used a file-scope `BlockScanLimit` of 500 after
commit `08f3100f2503b8d6750c1b65b31fe6ae055be84f` / #2725 raised LLVM 3.7's
default of 100 for a DSE case. The accompanying `-memdep-block-scan-limit`
option was routed to DSE, but the shared default also affected GVN's
memory-dependence queries. #9003 restores the upstream default and adds a
focused GVN test after the divergence caused a severe compile-time regression
on a large generated compute shader.
