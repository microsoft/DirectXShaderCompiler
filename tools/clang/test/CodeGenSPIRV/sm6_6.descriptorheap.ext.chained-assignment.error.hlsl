// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCASE=0 %s 2>&1 | FileCheck --check-prefix=IMAGE %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCASE=1 %s 2>&1 | FileCheck --check-prefix=BUFFER %s
// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCASE=2 %s | FileCheck --check-prefix=OK %s

// Verifies that a chained assignment whose innermost value is heap-sourced,
// e.g. `a = b = ResourceDescriptorHeap[0];`, is diagnosed instead of silently
// skipping alias tracking for `a` and falling back to plain resource-handle
// codegen. The RHS of the outer assignment is itself an assignment
// expression, which isHeapSourcedValue's DeclRefExpr/CallExpr-only
// resolution doesn't recognize, so without this diagnostic `a` never gets a
// heap-index alias and a later atomic on it hits
// VUID-StandaloneSpirv-OpTypeImage-06924 with no error message.
//
// CASE=2 is the control: a chained assignment where every source is a bound
// resource must NOT be rejected. That pattern never touches heap alias
// tracking at all and is already handled correctly by the normal assignment
// path.

// IMAGE:  assigning a heap-sourced value through a chained assignment is not supported
// BUFFER: assigning a heap-sourced value through a chained assignment is not supported
// OK:     OpImageWrite

RWByteAddressBuffer outputBytes : register(u0);
RWTexture2D<uint> boundTexA : register(u1);
RWTexture2D<uint> boundTexB : register(u2);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
#if CASE == 0
  RWTexture2D<uint> a, b;
  a = b = ResourceDescriptorHeap[0];
  uint r;
  InterlockedAdd(a[tid.xy], 1, r);
  outputBytes.Store(0, r);
#elif CASE == 1
  RWByteAddressBuffer a, b;
  a = b = ResourceDescriptorHeap[1];
  outputBytes.Store(0, a.Load(0));
#elif CASE == 2
  RWTexture2D<uint> a, b;
  a = b = boundTexA;
  a[tid.xy] = b[tid.xy];
#endif
}
