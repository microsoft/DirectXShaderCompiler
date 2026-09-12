// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s

// Verifies: returning a heap buffer alias from a function emits a clear
// diagnostic instead of producing invalid SPIR-V.
//
// Regression (B2 / C3): returning a StructuredBuffer alias variable caused
// loadIfGLValue to emit an OpLoad of the whole runtime-array struct through a
// StorageBuffer pointer, which is invalid SPIR-V.  The fix emits an actionable
// error before the load is attempted.
//
// Cross-function alias propagation (returning the alias and using it in the
// caller) is not yet implemented; it requires VariablePointersStorageBuffer so
// that the OpBufferPointerEXT value can survive an OpReturnValue boundary.
// Tracked as a follow-up; until then this path emits an error.
//
// Image aliases (RWTexture2D) are handled by the legalization pipeline (System A)
// and are not covered by this test.

// CHECK: heap buffer alias cannot be returned from a function

RWByteAddressBuffer outputBytes : register(u0);

StructuredBuffer<uint> makeAlias() {
  StructuredBuffer<uint> a = ResourceDescriptorHeap[40];
  StructuredBuffer<uint> b = ResourceDescriptorHeap[41];
  b = a;
  return b;
}

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  outputBytes.Store(0, makeAlias()[0]);
}
