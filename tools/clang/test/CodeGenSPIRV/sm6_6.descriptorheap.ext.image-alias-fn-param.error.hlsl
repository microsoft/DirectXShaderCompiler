// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_INOUT %s 2>&1 | FileCheck %s

// Verifies: a heap image alias passed to a function and used in an atomic is
// rejected, for both 'in' and 'inout' parameters.
//
// The argument is copied into a Function-class "param.var.*" variable and the
// callee's parameter is not an alias VarDecl, so the descriptor index cannot be
// recovered. The old fallback emitted OpImageTexelPointer on the parameter,
// which pinned it as a variable and kept an image-typed OpStore alive, failing
// validation with [VUID-StandaloneSpirv-OpTypeImage-06924] and no diagnostic.
//
// Plain reads and writes through the same parameter still work, because the
// loaded handle is sufficient — see alias-fn-readwrite.hlsl.
//
// TODO(#8784): accept once the descriptor index is propagated across function
// boundaries.

// CHECK: interlocked operation on a heap-backed RWTexture passed to or returned from a helper function is not supported

RWByteAddressBuffer outputBytes : register(u0);

#ifdef TEST_INOUT
void bump(inout RWTexture2D<uint> t, uint2 coord, out uint orig) {
#else
void bump(RWTexture2D<uint> t, uint2 coord, out uint orig) {
#endif
  InterlockedAdd(t[coord], 1, orig);
}

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWTexture2D<uint> tex = ResourceDescriptorHeap[40];
  uint r;
  bump(tex, tid.xy, r);
  outputBytes.Store(0, r);
}
