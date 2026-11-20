// RUN: %dxc -T ms_6_5 -E main -fspv-target-env=vulkan1.3 -fcgl %s -spirv | FileCheck %s

// Mesh shader outputs whose attributes are inherited from a base struct.

struct BaseAttrs {
  float3 Normal : NORMAL0;
};

struct VertexAttributes : BaseAttrs {
  float2 Uv : TEXCOORD0;
};

struct BaseOut {
  float3 Tangent : TANGENT0;
};

struct VertexOut : BaseOut {
  float4 PositionHS : SV_Position;
  VertexAttributes attributes;
};

// CHECK: OpDecorate %out_var_TANGENT0 Location 0
// CHECK: OpDecorate %out_var_NORMAL0 Location 1
// CHECK: OpDecorate %out_var_TEXCOORD0 Location 2

[outputtopology("triangle")]
[numthreads(3, 1, 1)]
void main(in uint tid : SV_GroupThreadID,
          out vertices VertexOut verts[3],
          out indices uint3 tris[1]) {
  SetMeshOutputCounts(3, 1);

  VertexOut vout = (VertexOut)0;

// Assigning the whole vertex writes the base class fields too, including the
// base class of the nested struct.
// CHECK:      [[base:%[0-9]+]] = OpCompositeExtract %BaseOut [[vout:%[0-9]+]] 0
// CHECK-NEXT: [[tan:%[0-9]+]] = OpCompositeExtract %v3float [[base]] 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v3float %out_var_TANGENT0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] [[tan]]
// CHECK:      [[attrs:%[0-9]+]] = OpCompositeExtract %VertexAttributes [[vout]] 2
// CHECK-NEXT: [[abase:%[0-9]+]] = OpCompositeExtract %BaseAttrs [[attrs]] 0
// CHECK-NEXT: [[nrm:%[0-9]+]] = OpCompositeExtract %v3float [[abase]] 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v3float %out_var_NORMAL0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] [[nrm]]
// CHECK-NEXT: [[uv:%[0-9]+]] = OpCompositeExtract %v2float [[attrs]] 1
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v2float %out_var_TEXCOORD0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] [[uv]]
  verts[tid] = vout;

// Assigning inherited fields directly.
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v3float %out_var_TANGENT0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] {{%[0-9]+}}
  verts[tid].Tangent = float3(1, 0, 0);
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v3float %out_var_NORMAL0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] {{%[0-9]+}}
  verts[tid].attributes.Normal = float3(0, 0, 1);
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_NORMAL0 {{%[0-9]+}} %uint_1
// CHECK-NEXT:                   OpStore [[ptr]] %float_2
  verts[tid].attributes.Normal.y = 2.0;

  tris[0] = uint3(0, 1, 2);
}
