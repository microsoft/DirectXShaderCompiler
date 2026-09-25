// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck --check-prefix=FORWARD %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_REASSIGN %s 2>&1 | FileCheck --check-prefix=REASSIGN %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_LOCAL_REASSIGN_FROM_CALL %s 2>&1 | FileCheck --check-prefix=LOCAL %s

// Verifies three cases that must all be diagnosed the same as a direct
// `return <var initialized from ResourceDescriptorHeap>;`, not silently fall
// through to an invalid OpImageTexelPointer:
//
// Case FORWARD - a function's return value is itself a call to another
//   function that returns a heap-sourced image
//   (functionReturnsHeapSourcedImage's CallExpr case).
// Case REASSIGN - the returned variable is heap-sourced via a later
//   assignment rather than its initializer
//   (isExprStaticallyHeapSourcedImage's anyAssignmentToVarSatisfies
//   fallback).
// Case LOCAL - a local is reassigned from a call, then used in an atomic in
//   the SAME function as the reassignment
//   (isDescriptorHeapImageBoundaryLoss's own anyAssignmentToVarSatisfies
//   fallback, not functionReturnsHeapSourcedImage's). This must stay
//   distinct from a local reassigned directly from a heap subscript, which
//   is NOT a loss (see sm6_6.descriptorheap.ext.rwtexture-atomics.hlsl's
//   reassignment case).

// FORWARD: interlocked operation on a heap-backed RWTexture passed to or returned from a helper function is not supported
// REASSIGN: interlocked operation on a heap-backed RWTexture passed to or returned from a helper function is not supported
// LOCAL: interlocked operation on a heap-backed RWTexture passed to or returned from a helper function is not supported

RWByteAddressBuffer outputBytes : register(u0);

RWTexture2D<uint> makeAliasCore(uint slot) {
  RWTexture2D<uint> tex = ResourceDescriptorHeap[slot];
  return tex;
}

#if defined(TEST_REASSIGN)
RWTexture2D<uint> makeAlias(uint slot) {
  RWTexture2D<uint> tex;
  tex = ResourceDescriptorHeap[slot];
  return tex;
}
#elif !defined(TEST_LOCAL_REASSIGN_FROM_CALL)
// Forwards makeAliasCore's heap-sourced return value without touching a
// ResourceDescriptorHeap subscript itself.
RWTexture2D<uint> makeAlias(uint slot) { return makeAliasCore(slot); }
#endif

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint r;
#ifdef TEST_LOCAL_REASSIGN_FROM_CALL
  RWTexture2D<uint> tex;
  tex = makeAliasCore(40);
#else
  RWTexture2D<uint> tex = makeAlias(40);
#endif
  InterlockedAdd(tex[tid.xy], 1, r);
  outputBytes.Store(0, r);
}
