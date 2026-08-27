# DirectX Shader Compiler Redistributable Package

This package contains a copy of the DirectX Shader Compiler redistributable and its associated development headers.

For help getting started, please see:

<https://github.com/microsoft/DirectXShaderCompiler/wiki>

## Licenses

The included licenses apply to the following files:

| License file | Applies to |
|---|---|
|LICENSE-MIT.txt    |d3d12shader.h|
|LICENSE-LLVM.txt   |all other files|

## Changelog

This file contains notes for releases that are not published.
[GitHub Releases](https://github.com/microsoft/DirectXShaderCompiler/releases)
contain notes for published releases.

Use **Upcoming Release** for all non-experimental changes. Use this section
even if a change is first available in a preview build.

Use **Upcoming Preview Release** only for changes to experimental features. If
an experimental feature becomes supported, add a note to **Upcoming Release**.

### Upcoming Release

#### HLSL Language

- Casting a scalar to a struct or array containing a resource is now an error
  instead of crashing
  [#6661](https://github.com/microsoft/DirectXShaderCompiler/issues/6661).

#### Bug Fixes

- Fixed derivative operations being moved into divergent control flow, which
  could produce incorrect results
  [#8001](https://github.com/microsoft/DirectXShaderCompiler/issues/8001).
- SPIR-V: Fixed an invalid `OpSelect` being generated when optimizing for
  SPIR-V 1.3 and earlier
  [#8603](https://github.com/microsoft/DirectXShaderCompiler/issues/8603).
- Fix a crash generating DXIL from sources containing a dynamic resource heap
  access that was discarded. Identified during development of SPIR-V support for
  [descriptor heaps](https://github.com/microsoft/DirectXShaderCompiler/pull/8517#discussion_r3752113078).
- Fixed internal compiler errors when a member method is called on a ray payload
  or on one of its fields with payload access qualifiers enabled
  [#6464](https://github.com/microsoft/DirectXShaderCompiler/issues/6464).

### Upcoming Preview Release

These changes apply to experimental preview shader models only and will not be
part of the next non-preview release.

#### Experimental Shader Model 6.10

These are incremental changes to the experimental Shader Model 6.10 features that
first shipped in the 1.10.2605 preview.

- Fixed the set of numeric types allowed in LinAlg matrix intrinsics
  [#8271](https://github.com/microsoft/DirectXShaderCompiler/issues/8271).
- Corrected the parameter order of `InterlockedAccumulate`
  [#8459](https://github.com/microsoft/DirectXShaderCompiler/pull/8459).
- Added validation of LinAlg matrix builtin parameters and result K dimension
  [#8588](https://github.com/microsoft/DirectXShaderCompiler/pull/8588).
- Restricted the component types allowed in LinAlg matrices
  [#8608](https://github.com/microsoft/DirectXShaderCompiler/pull/8608).
- Added `BFloat16` to the ComponentType enum in DxilConstants and the linalg
  header [#8722](https://github.com/microsoft/DirectXShaderCompiler/issues/8722)
