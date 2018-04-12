// Run: %dxc -T cs_6_0 -E main

// CHECK: OpEntryPoint GLCompute %main "main" %gl_WorkGroupID
// CHECK: OpDecorate %gl_WorkGroupID BuiltIn WorkgroupId
// CHECK: %gl_WorkGroupID = OpVariable %_ptr_Input_v3int Input

// CHECK:         %param_var_tid = OpVariable %_ptr_Function_v2int Function
// CHECK: [[gl_WorkGrouID:%\d+]] = OpLoad %v3int %gl_WorkGroupID
// CHECK:  [[int2_GroupID:%\d+]] = OpVectorShuffle %v2int [[gl_WorkGrouID]] [[gl_WorkGrouID]] 0 1
// CHECK:                          OpStore %param_var_tid [[int2_GroupID]]

[numthreads(8, 8, 8)]
void main(int2 tid : SV_GroupID) {}
