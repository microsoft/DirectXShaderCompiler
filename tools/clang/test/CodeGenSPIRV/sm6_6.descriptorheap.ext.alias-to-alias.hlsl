// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies that alias-to-alias assignment (sb2 = sb1) and copy-init (sb3 = sb1)
// propagate the source's descriptor slot to the destination.
//
// Propagation is compile-time: tryToAssignDescriptorHeapBufferAlias copies the
// source's index SpirvInstruction into the destination alias; legalization then
// constant-folds the index-variable loads away. As a result the overwritten
// slot (%uint_31, %uint_21) appears only in the dead declaration-site access
// chain, never at a use site. A second occurrence means propagation failed.
//
// Regression: alias-var sources previously crashed in getDeclEvalInfo because
// buffer alias VarDecls are not registered in astDecls.

struct Payload { uint value; };
RWByteAddressBuffer outputBytes : register(u0);

// ---- StructuredBuffer section ----------------------------------------
// sb1->slot 30, sb2->slot 31 (overwritten to 30 by sb2=sb1), sb3 copy-init from sb1.
//
// sb2's dead declaration chain is the ONLY %uint_31 StructuredBuffer chain:
// CHECK: OpUntypedAccessChainKHR %type_untyped_pointer %_runtimearr_type_buffer_ext_0 %resource_heap %uint_31
// CHECK-NOT: OpUntypedAccessChainKHR %type_untyped_pointer %_runtimearr_type_buffer_ext_0 %resource_heap %uint_31
//
// sb2[0] and sb3[0] produce element loads (via propagated slot 30):
// CHECK: OpLoad %uint
// CHECK: OpLoad %uint

// ---- ConstantBuffer section ------------------------------------------
// cb1->slot 20, cb2->slot 21 (overwritten to 20 by cb2=cb1).
//
// cb2's dead declaration chain is the ONLY %uint_21 ConstantBuffer chain:
// CHECK: OpUntypedAccessChainKHR %type_untyped_pointer %_runtimearr_type_buffer_ext %resource_heap %uint_21
// CHECK-NOT: OpUntypedAccessChainKHR %type_untyped_pointer %_runtimearr_type_buffer_ext %resource_heap %uint_21
//
// cb2.value produces a member load (via propagated slot 20):
// CHECK: OpLoad %uint

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint r0, r1;

  StructuredBuffer<uint> sb1 = ResourceDescriptorHeap[30];
  StructuredBuffer<uint> sb2 = ResourceDescriptorHeap[31];
  sb2 = sb1;           // alias assign: sb2 now uses sb1's slot (30)

  StructuredBuffer<uint> sb3 = sb1;  // copy-init: sb3 uses sb1's slot (30)

  r0 = sb2[0];         // must use slot 30
  r0 += sb3[0];        // must use slot 30

  ConstantBuffer<Payload> cb1 = ResourceDescriptorHeap[20];
  ConstantBuffer<Payload> cb2 = ResourceDescriptorHeap[21];
  cb2 = cb1;           // alias assign: cb2 now uses cb1's slot (20)

  r1 = cb2.value;      // must use slot 20

  outputBytes.Store(0, r0);
  outputBytes.Store(4, r1);
}
