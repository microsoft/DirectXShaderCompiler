// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck --check-prefix=GLOBAL %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_STATIC_LOCAL %s 2>&1 | FileCheck --check-prefix=STATIC_LOCAL %s

// Verifies: an atomic on a file-scope (static global) or static-local
// RWTexture initialized from ResourceDescriptorHeap is rejected, instead of
// silently falling through to an invalid OpImageTexelPointer on a
// Private-class variable (VUID-StandaloneSpirv-OpTypeImage-06924, no
// diagnostic).
//
// doVarDecl only calls tryToAssignDescriptorHeapImageAlias /
// tryToCreateDescriptorHeapAlias in the local-storage branch; neither ever
// runs for a file-scope declaration, so its heap index is never tracked. A
// plain (non-atomic) heap-sourced file-scope RWTexture/StructuredBuffer is
// valid and unaffected -- see static-global.hlsl.

// GLOBAL: interlocked operation on a heap-backed RWTexture must read the descriptor directly at this call site
// STATIC_LOCAL: interlocked operation on a heap-backed RWTexture must read the descriptor directly at this call site

RWByteAddressBuffer outputBytes : register(u0);

#ifdef TEST_STATIC_LOCAL

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  static RWTexture2D<uint> g = ResourceDescriptorHeap[0];
  uint orig;
  InterlockedAdd(g[tid.xy], 1, orig);
  outputBytes.Store(0, orig);
}

#else

static RWTexture2D<uint> g = ResourceDescriptorHeap[0];

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint orig;
  InterlockedAdd(g[tid.xy], 1, orig);
  outputBytes.Store(0, orig);
}

#endif
