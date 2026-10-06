# DXC agent guidance

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
