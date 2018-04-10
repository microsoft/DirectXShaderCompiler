// Run: %dxc -T cs_6_0 -E main

// CHECK: OpEntryPoint GLCompute %main "main" %gl_LocalInvocationID
// CHECK: OpDecorate %gl_LocalInvocationID BuiltIn LocalInvocationId
// CHECK: %gl_LocalInvocationID = OpVariable %_ptr_Input_v3uint Input

// CHECK: [[gl_LocalInvocationID:%\d+]] = OpLoad %v3uint %gl_LocalInvocationID
// CHECK:  [[uint2_GroupThreadID:%\d+]] = OpVectorShuffle %v2uint [[gl_LocalInvocationID]] [[gl_LocalInvocationID]] 0 1
// CHECK:                                 OpStore %param_var_gtid [[uint2_GroupThreadID]]

[numthreads(8, 8, 8)]
void main(uint2 gtid : SV_GroupThreadID) {}
