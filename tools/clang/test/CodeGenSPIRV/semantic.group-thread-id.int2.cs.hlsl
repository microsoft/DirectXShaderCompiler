// Run: %dxc -T cs_6_0 -E main

// CHECK: OpEntryPoint GLCompute %main "main" %gl_LocalInvocationID
// CHECK: OpDecorate %gl_LocalInvocationID BuiltIn LocalInvocationId
// CHECK: %gl_LocalInvocationID = OpVariable %_ptr_Input_v3int Input

// CHECK:               %param_var_gtid = OpVariable %_ptr_Function_v2int Function
// CHECK: [[gl_LocalInvocationID:%\d+]] = OpLoad %v3int %gl_LocalInvocationID
// CHECK:   [[int2_GroupThreadID:%\d+]] = OpVectorShuffle %v2int [[gl_LocalInvocationID]] [[gl_LocalInvocationID]] 0 1
// CHECK:                                 OpStore %param_var_gtid [[int2_GroupThreadID]]

[numthreads(8, 8, 8)]
void main(int2 gtid : SV_GroupThreadID) {}
