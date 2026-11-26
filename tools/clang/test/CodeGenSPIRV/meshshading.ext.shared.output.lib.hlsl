// RUN: %dxc -T lib_6_6 -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s

// Two mesh entry points that share output types. Each one gets its own stage
// variables and must write to those, not to the other entry point's.

struct Inner {
  float4 a : COLOR1;
};

struct VertOut {
  float4 pos : SV_Position;
  float4 flat : COLOR0;
  Inner nested;
};

struct PrimOut {
  float4 flat : COLOR2;
  Inner nested : COLOR3;
};

// CHECK: OpEntryPoint MeshEXT %ms1 "ms1" {{.*}}%out_var_COLOR0 %out_var_COLOR1 %out_var_COLOR2 %out_var_COLOR3
// CHECK: OpEntryPoint MeshEXT %ms2 "ms2" {{.*}}%out_var_COLOR0_0 %out_var_COLOR1_0 %out_var_COLOR2_0 %out_var_COLOR3_0

// CHECK: %ms1 = OpFunction
// CHECK-NOT: OpFunctionEnd
// CHECK-DAG: OpAccessChain %_ptr_Output_v4float %out_var_COLOR0 %int_0
// CHECK-DAG: OpAccessChain %_ptr_Output_v4float %out_var_COLOR1 %int_0
// CHECK-DAG: OpAccessChain %_ptr_Output_v4float %out_var_COLOR2 %int_0
// CHECK-DAG: OpAccessChain %_ptr_Output_v4float %out_var_COLOR3 %int_0
// CHECK: OpFunctionEnd
[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void ms1(out vertices VertOut verts[3], out primitives PrimOut prims[1],
         out indices uint3 tris[1]) {
  SetMeshOutputCounts(3, 1);
  VertOut v = (VertOut)0;
  verts[0] = v;
  PrimOut p = (PrimOut)0;
  prims[0] = p;
  tris[0] = uint3(0, 1, 2);
}

// CHECK: %ms2 = OpFunction
// CHECK-NOT: OpFunctionEnd
// CHECK-DAG: OpAccessChain %_ptr_Output_v4float %out_var_COLOR0_0 %int_0
// CHECK-DAG: OpAccessChain %_ptr_Output_v4float %out_var_COLOR1_0 %int_0
// CHECK-DAG: OpAccessChain %_ptr_Output_v4float %out_var_COLOR2_0 %int_0
// CHECK-DAG: OpAccessChain %_ptr_Output_v4float %out_var_COLOR3_0 %int_0
// CHECK: OpFunctionEnd
[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void ms2(out vertices VertOut verts[3], out primitives PrimOut prims[1],
         out indices uint3 tris[1]) {
  SetMeshOutputCounts(3, 1);
  VertOut v = (VertOut)1;
  verts[0] = v;
  PrimOut p = (PrimOut)1;
  prims[0] = p;
  tris[0] = uint3(0, 1, 2);
}
