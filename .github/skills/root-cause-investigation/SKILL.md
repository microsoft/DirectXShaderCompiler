---
name: root-cause-investigation
description: Investigate DXC correctness or performance issues by checking local fork history and globally shared tuning before proposing a new fix.
---

# Root-cause investigation

Use this when an issue is slow, miscompiles, or behaves unexpectedly in DXC. Start from the exact hot path and the specific behavior, not from the explanation you want to be true.

## Investigation checklist

- Start with the hot function and enumerate every limit, cutoff, and heuristic that governs it. If performance is the symptom, find the exact pass and function before proposing a new cap or mechanism.
- Check the local DXC fork before comparing to upstream. DXC is based on LLVM 3.7 and has local divergences marked with `HLSL Change`; a surprising constant is more likely a fork-specific regression than a fundamental LLVM bug.
- Run `git blame`, `git log -p`, and `git log -S<symbol>` on the hot file or constant before designing a fix. Treat a suspicious tuning constant as a regression candidate before a design problem.
- Beware globally-scoped tuning constants. A limit raised for one pass often affects every consumer of the same analysis. When changing a shared constant, enumerate all users and say which ones are affected.
- Prefer restoring the upstream default over adding a new DXC-specific cap or heuristic. A new mechanism in shared LLVM analysis code carries correctness risk that this repo's IR pass tests do not reliably cover.
- Understand the repo's actual test coverage before changing core LLVM analyses or passes. In this fork, `test/Transforms/lit.local.cfg` and `test/Analysis/lit.local.cfg` both set `config.suffixes = []`, so the upstream IR pass suites are effectively disabled. Add narrow DXC tests under `tools/clang/test/DXC/Passes/` when needed, but do not treat them as a substitute for the missing LLVM suite.
- Do not let a proposed fix shape the repro. Reproduce the original issue's behavior and sensitivity to the relevant knobs before trusting a minimal reproducer. A clean-room repro that ignores the controlling limit or condition is not evidence of the real root cause.
- If an experiment produces a surprising result, stop and re-derive the root cause instead of continuing the same plan.
- Cross-fork comparisons answer "how does upstream do this today"; local git history answers "what did this fork diverge from." Do the local-history check first.

## Worked example

A useful example in this repo is the `MemoryDependenceAnalysis` scan-limit regression:

- `lib/Analysis/MemoryDependenceAnalysis.cpp` had `BlockScanLimit = 500`.
- `git blame` points at commit `08f3100f2503b8d6750c1b65b31fe6ae055be84f` (`Increase scan limit for DSE, add option (#2725)`), which raised the value from LLVM 3.7's default of 100.
- The option was plumbed as `-memdep-block-scan-limit` and used by DSE, but the constant is shared by `MemoryDependenceAnalysis` and therefore affects GVN and other analysis consumers.
- PR #9003 (`Restore LLVM 3.7 MemoryDependenceAnalysis default`) restores 100 and adds targeted coverage in `tools/clang/test/DXC/Passes/GVN/`.
- On a large generated compute shader, restoring the default reduced compile time from roughly 36.75s to 6.48s, which is strong evidence that the regression was in the local DXC fork, not upstream LLVM.

This is the pattern to follow: local history first, then upstream comparison, then a narrowly justified change.

## Release note note

This skill is documentation for contributors and agents, not a user-visible compiler feature or behavior change. Under `CONTRIBUTING.md`, release notes are expected for significant compiler-visible behavior changes, but a docs-only investigation guide does not normally require a `docs/ReleaseNotes.md` entry unless the same change alters user-visible compiler behavior.
