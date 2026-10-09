// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_STRUCTURED %s 2>&1 | FileCheck --check-prefix=SB %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck --check-prefix=BAB %s

// Verifies that passing a heap buffer alias to a user function emits a
// diagnostic instead of crashing (buffer alias VarDecls are not in astDecls,
// so getDeclEvalInfo would fault).
//
// TODO: remove once VariablePointersStorageBuffer-based pass-by-value is
// implemented and buffer aliases can be passed to functions.

// SB:  heap buffer alias cannot be passed to a user function
// BAB: heap buffer alias cannot be passed to a user function

RWByteAddressBuffer outputBytes : register(u0);

#ifdef TEST_STRUCTURED
uint consume(StructuredBuffer<uint> buf) { return buf[0]; }
#else
uint consume(ByteAddressBuffer buf) { return buf.Load(0); }
#endif

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
#ifdef TEST_STRUCTURED
  StructuredBuffer<uint> sb = ResourceDescriptorHeap[0];
  outputBytes.Store(0, consume(sb));
#else
  ByteAddressBuffer bab = ResourceDescriptorHeap[0];
  outputBytes.Store(0, consume(bab));
#endif
}
