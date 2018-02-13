// Run: %dxc -T cs_6_0 -E main

RWStructuredBuffer<uint> values;

// CHECK: OpCapability SubgroupBallotKHR
// CHECK: OpExtension "SPV_KHR_shader_ballot"

// CHECK: OpEntryPoint GLCompute
// CHECK-SAME: %SubgroupSize

// CHECK: OpDecorate %SubgroupSize BuiltIn SubgroupSize

// CHECK: %SubgroupSize = OpVariable %_ptr_Input_uint Input

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
// CHECK: OpLoad %uint %SubgroupSize
    values[id.x] = WaveGetLaneCount();
}
