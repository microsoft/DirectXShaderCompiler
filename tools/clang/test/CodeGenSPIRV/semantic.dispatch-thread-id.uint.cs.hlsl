// Run: %dxc -T cs_6_0 -E main

// CHECK: OpEntryPoint GLCompute %main "main" %gl_GlobalInvocationID
// CHECK: OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId
// CHECK: %gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input

// CHECK: [[gl_GlobalInvocationID:%\d+]] = OpLoad %v3uint %gl_GlobalInvocationID
// CHECK: [[uint_DispatchThreadID:%\d+]] = OpCompositeExtract %uint [[gl_GlobalInvocationID]] 0
// CHECK:                                  OpStore %param_var_tid [[uint_DispatchThreadID]]

[numthreads(8, 8, 8)]
void main(uint tid : SV_DispatchThreadId) {}
