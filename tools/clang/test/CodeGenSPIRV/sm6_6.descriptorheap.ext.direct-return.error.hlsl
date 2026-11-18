// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_EXPLICIT_CAST %s 2>&1 | FileCheck %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_BYTEADDRESS %s 2>&1 | FileCheck %s

// Verifies: returning a direct heap subscript for a buffer-like type is
// rejected, in both the implicit and explicit cast forms.
//
// This form has no alias VarDecl, so it skips the alias-return guard in
// alias-return.hlsl. It used to carry the OpBufferPointerEXT value across
// OpReturnValue without VariablePointersStorageBuffer, producing invalid
// SPIR-V with no diagnostic at all.
//
// TODO(#8784): accept once cross-function heap buffer propagation is
// implemented.

// CHECK: heap buffer cannot be returned from a function

RWByteAddressBuffer outputBytes : register(u0);

#ifdef TEST_BYTEADDRESS
ByteAddressBuffer get() { return ResourceDescriptorHeap[40]; }
#elif defined(TEST_EXPLICIT_CAST)
StructuredBuffer<uint> get() {
  return (StructuredBuffer<uint>)ResourceDescriptorHeap[40];
}
#else
StructuredBuffer<uint> get() { return ResourceDescriptorHeap[40]; }
#endif

[numthreads(1, 1, 1)]
void main() {
#ifdef TEST_BYTEADDRESS
  outputBytes.Store(0, get().Load(0));
#else
  outputBytes.Store(0, get()[0]);
#endif
}
