---
name: code-review
description: Pull request review guidance for the DirectX Shader Compiler. Use when reviewing a pull request in this repository. Sets review priorities (correctness, memory safety, compiler compatibility, tests, design, conventions), and specifies that wording, spelling, grammar, and terminology findings should be reported as inline comments rather than suppressed. Also covers when release notes are required.
---

# Reviewing pull requests in DirectXShaderCompiler

## Priorities

This skill *adds* to your normal review; it does not replace it. Continue to
apply your full correctness, security, and design analysis. The wording rules
below are an addition at the bottom of the priority list, never a substitute for
the rest.

Review in this order, and let the earlier categories dominate the review:

1. **Correctness and behavior.** Miscompiles, wrong codegen, incorrect
   diagnostics, broken or missing validation, logic errors, off-by-one errors,
   incorrect control flow.
2. **Memory and lifetime safety.** Null and uninitialized dereferences, buffer
   and index overruns, use-after-free, leaks, ownership mistakes, unchecked
   casts, and misuse of LLVM ADT and `Value`/`Instruction` lifetimes.
3. **Compatibility and compiler contracts.** Changes to accepted HLSL, shader
   model gating, DXIL validator version constraints, SPIR-V codegen behavior,
   command-line option semantics, and anything that silently changes output for
   existing shaders.
4. **Test coverage.** Whether the change is tested, and whether the tests
   actually exercise the new behavior rather than just running it.
5. **Design and maintainability.** Layering violations, duplicated logic,
   misplaced abstractions, and code that conflicts with surrounding patterns.
6. **Project conventions.** LLVM Coding Standards and clang-format compliance
   for changed C and C++ code, per `CONTRIBUTING.md` and
   `docs/CodingStandards.rst`; HLSL-specific guidance in `docs/HLSLChanges.rst`.
7. **Release notes.** See the section at the end of this file.
8. **Wording.** Spelling, grammar, terminology, and punctuation consistency, as
   described next.

If a pull request has correctness problems, lead with those. Wording comments
must never crowd out or delay a substantive finding.

## Report wording issues instead of suppressing them

This repository *wants* small wording feedback. Do not filter out spelling,
grammar, terminology, or punctuation-consistency findings on the grounds that
they are low impact, cosmetic, or "nitpicks". If you would otherwise place such
a finding in a "Suppressed comments" section of the review summary, post it as a
normal inline review comment instead.

This applies **everywhere in the diff**, including:

- Markdown and reStructuredText docs (`docs/`, `README.md`, `CONTRIBUTING.md`)
- Issue and pull request templates (`.github/ISSUE_TEMPLATE/`)
- Code comments and Doxygen blocks in C, C++, and HLSL sources
- Identifiers: type, function, variable, and macro names
- User-facing strings: diagnostic and error messages, command-line help text,
  and `-help` / usage output
- Test file comments and RUN/CHECK line descriptions

Report these categories:

- **Spelling errors and typos**, for example `bellow` → `below`,
  `recieve` → `receive`, `seperate` → `separate`.
- **Grammar errors**, for example subject/verb disagreement such as
  "the release branch don't exist" → "the release branch doesn't exist", or
  awkward prepositions such as "kept into sync" → "kept in sync".
- **Terminology and brand capitalization**, using the table below.
- **Punctuation and formatting inconsistency within a single list, table, or
  block** — for example a checklist where some items end in a period and others
  do not, or a bullet list with inconsistent capitalization of the first word.

Prefer a GitHub suggested change so the author can apply the fix in one click.

### Terminology and capitalization

| Use | Not |
| --- | --- |
| `NuGet` | `Nuget`, `nuget` (outside package IDs and CLI invocations) |
| `GitHub` | `github`, `Github` (outside URLs, paths, and org/repo names) |
| `SPIR-V` | `SPIRV`, `Spir-V`, `spirv` (outside identifiers and file paths) |
| `HLSL` | `hlsl` (outside file extensions and paths) |
| `DXIL` | `Dxil`, `dxil` (outside identifiers, file names, and paths) |
| `DXC` | `dxc` when referring to the project rather than the executable |
| `DirectX` | `Directx`, `directx` |
| `LunarG` | `Lunarg`, `lunarg` |
| `Vulkan` | `vulkan` |
| `LLVM` / `Clang` | `llvm` / `clang` when referring to the projects in prose |
| `macOS` | `MacOS`, `OSX` |

Do not flag a casing "violation" when the lowercase form is load-bearing: URLs,
file paths, file extensions, code identifiers, CLI flags, environment variables,
and literal command invocations must be left exactly as written.

### Keep wording comments proportionate

Report these findings, but keep them clearly secondary to correctness:

- Mark wording comments as minor, for example by prefixing them with `Nit:`.
- Group repeats. If the same typo or casing issue appears more than three times
  in one file, leave one comment naming the pattern and the affected lines
  rather than one comment per occurrence.
- Comment only on lines the pull request actually adds or modifies. Do not
  flag pre-existing wording in untouched context lines.
- Do not rewrite intentional style. This project follows the
  [LLVM Coding Standards](../../../docs/CodingStandards.rst); do not propose
  wording changes that conflict with the conventions already used in the file.

## Release notes

Check whether `docs/ReleaseNotes.md` should be updated, following the "Release
Notes" policy in `CONTRIBUTING.md`.

- Release notes are expected for user-visible, significant compiler behavior
  changes: new language or hardware features, new compiler options, important
  isolated bug fixes, and changes in default behavior.
- Release notes are usually not needed for refactors, test-only updates, or
  infrastructure-only changes, unless user-visible behavior changes.
- Account for multi-pull-request efforts. If a pull request is one part of a
  larger tracked effort, a single shared release note may be intentional;
  confirm coverage exists or is planned across the effort rather than requiring
  a duplicate entry per pull request.

Do **not** leave a release note comment when the pull request already updates
`docs/ReleaseNotes.md`, is docs-only, or is a dependency bump such as a
"Bump ..." pull request.

When a release note is clearly warranted and missing, say so directly and point
to `docs/ReleaseNotes.md`. When it is not obvious, ask only the gentle version:
**"Did you consider adding a release note?"**
