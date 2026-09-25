// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_DIRECT_USE %s 2>&1 | FileCheck %s

// Verifies: a heap image alias obtained from a function return value and then
// used in an atomic is rejected, for both assignment-then-atomic and direct
// use of the return value.
//
// When the image is returned from a helper the descriptor-index slot is not
// propagated to the caller.  The old code fell through to the bound-resource
// fallback which emits OpImageTexelPointer on a Function-class image pointer,
// failing SPIR-V validation with
// [VUID-StandaloneSpirv-OpTypeImage-06924] and no diagnostic.
//
// functionReturnsHeapSourcedImage scans the callee's AST for a return
// statement whose value is statically heap-sourced, independent of codegen
// order (the callee may not have been emitted yet): diagnose before
// emitting whenever that scan says yes.
//
// Plain reads and writes through the returned alias still work because
// OpImageRead/OpImageWrite only need the handle value; only atomics
// (OpImageTexelPointer) are affected by this limitation.
//
// TODO(#8784): accept once the descriptor index is propagated across function
// boundaries.

// CHECK: interlocked operation on a heap-backed RWTexture passed to or returned from a helper function is not supported

RWByteAddressBuffer outputBytes : register(u0);

RWTexture2D<uint> makeAlias(uint slot) {
  RWTexture2D<uint> tex = ResourceDescriptorHeap[slot];
  return tex;
}

#ifndef TEST_DIRECT_USE
// Case 1: assign return value to local, then atomic on local.
[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWTexture2D<uint> tex = makeAlias(40);
  uint r;
  InterlockedAdd(tex[tid.xy], 1, r);
  outputBytes.Store(0, r);
}
#else
// Case 2: atomic directly on the return-value subscript.
[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint r;
  InterlockedAdd(makeAlias(40)[tid.xy], 1, r);
  outputBytes.Store(0, r);
}
#endif
