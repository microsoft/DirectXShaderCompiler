// Run: %dxc -T cs_6_0 -E main

// CHECK: OpEntryPoint GLCompute %main "main" %gl_WorkGroupID
// CHECK: OpDecorate %gl_WorkGroupID BuiltIn WorkgroupId
// CHECK: %gl_WorkGroupID = OpVariable %_ptr_Input_v3uint Input

// CHECK: [[gl_WorkGrouID:%\d+]] = OpLoad %v3uint %gl_WorkGroupID
// CHECK: [[uint2_GroupID:%\d+]] = OpVectorShuffle %v2uint [[gl_WorkGrouID]] [[gl_WorkGrouID]] 0 1
// CHECK:                          OpStore %param_var_tid [[uint2_GroupID]]

[numthreads(8, 8, 8)]
void main(uint2 tid : SV_GroupID) {}
