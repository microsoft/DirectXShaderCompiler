// Run: %dxc -T cs_6_0 -E main

// CHECK: OpEntryPoint GLCompute %main "main" %gl_LocalInvocationID
// CHECK: OpDecorate %gl_LocalInvocationID BuiltIn LocalInvocationId
// CHECK: %gl_LocalInvocationID = OpVariable %_ptr_Input_v3uint Input

void main(uint3 gtid : SV_GroupThreadID) : A {}