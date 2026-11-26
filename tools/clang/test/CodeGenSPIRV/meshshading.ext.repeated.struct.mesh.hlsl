// RUN: %dxc -T ms_6_5 -E main -fspv-target-env=vulkan1.3 -fcgl %s -spirv | FileCheck %s

// Mesh shader outputs that use the same struct type more than once. Each use
// has its own stage variables, even though the field decls are the same.

struct Pair {
  float x;
  float y;
};

// The fields inherit the semantic of the struct: A0, A1 and B0, B1.
struct Attributes {
  Pair p0 : A0;
  Pair p1 : B0;
};

// CHECK-DAG: OpDecorate %out_var_A0_0 PerPrimitiveEXT
// CHECK-DAG: OpDecorate %out_var_A1_0 PerPrimitiveEXT
// CHECK-DAG: OpDecorate %out_var_B0_0 PerPrimitiveEXT
// CHECK-DAG: OpDecorate %out_var_B1_0 PerPrimitiveEXT
// CHECK-DAG: OpDecorate %out_var_A0 Location 0
// CHECK-DAG: OpDecorate %out_var_A1 Location 1
// CHECK-DAG: OpDecorate %out_var_B0 Location 2
// CHECK-DAG: OpDecorate %out_var_B1 Location 3
// CHECK-DAG: OpDecorate %out_var_A0_0 Location 4
// CHECK-DAG: OpDecorate %out_var_A1_0 Location 5
// CHECK-DAG: OpDecorate %out_var_B0_0 Location 6
// CHECK-DAG: OpDecorate %out_var_B1_0 Location 7

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(out vertices Attributes verts[3],
          out primitives Attributes prims[1],
          out indices uint3 tris[1]) {
  SetMeshOutputCounts(3, 1);

// The same field of the two members.
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_A0 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] %float_1
  verts[0].p0.x = 1.0;
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_B0 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] %float_2
  verts[0].p1.x = 2.0;

// The same fields again, in the per-primitive output.
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_A1_0 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] %float_3
  prims[0].p0.y = 3.0;
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_B1_0 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] %float_4
  prims[0].p1.y = 4.0;

  Pair pair;
  pair.x = 5.0;
  pair.y = 6.0;

// Assigning each member as a whole.
// CHECK:      [[x:%[0-9]+]] = OpCompositeExtract %float [[p0:%[0-9]+]] 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_A0 %int_1
// CHECK-NEXT:                   OpStore [[ptr]] [[x]]
// CHECK-NEXT: [[y:%[0-9]+]] = OpCompositeExtract %float [[p0]] 1
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_A1 %int_1
// CHECK-NEXT:                   OpStore [[ptr]] [[y]]
  verts[1].p0 = pair;
// CHECK:      [[x:%[0-9]+]] = OpCompositeExtract %float [[p1:%[0-9]+]] 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_B0 %int_1
// CHECK-NEXT:                   OpStore [[ptr]] [[x]]
// CHECK-NEXT: [[y:%[0-9]+]] = OpCompositeExtract %float [[p1]] 1
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_B1 %int_1
// CHECK-NEXT:                   OpStore [[ptr]] [[y]]
  verts[1].p1 = pair;

  Attributes attrs = (Attributes)0;

// Assigning the whole vertex.
// CHECK:      [[p0:%[0-9]+]] = OpCompositeExtract %Pair [[attrs:%[0-9]+]] 0
// CHECK-NEXT: [[x:%[0-9]+]] = OpCompositeExtract %float [[p0]] 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_A0 %int_2
// CHECK-NEXT:                   OpStore [[ptr]] [[x]]
// CHECK-NEXT: [[y:%[0-9]+]] = OpCompositeExtract %float [[p0]] 1
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_A1 %int_2
// CHECK-NEXT:                   OpStore [[ptr]] [[y]]
// CHECK-NEXT: [[p1:%[0-9]+]] = OpCompositeExtract %Pair [[attrs]] 1
// CHECK-NEXT: [[x:%[0-9]+]] = OpCompositeExtract %float [[p1]] 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_B0 %int_2
// CHECK-NEXT:                   OpStore [[ptr]] [[x]]
// CHECK-NEXT: [[y:%[0-9]+]] = OpCompositeExtract %float [[p1]] 1
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_B1 %int_2
// CHECK-NEXT:                   OpStore [[ptr]] [[y]]
  verts[2] = attrs;

// And the whole primitive.
// CHECK:      [[p0:%[0-9]+]] = OpCompositeExtract %Pair [[attrs:%[0-9]+]] 0
// CHECK-NEXT: [[x:%[0-9]+]] = OpCompositeExtract %float [[p0]] 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_A0_0 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] [[x]]
// CHECK-NEXT: [[y:%[0-9]+]] = OpCompositeExtract %float [[p0]] 1
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_A1_0 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] [[y]]
// CHECK-NEXT: [[p1:%[0-9]+]] = OpCompositeExtract %Pair [[attrs]] 1
// CHECK-NEXT: [[x:%[0-9]+]] = OpCompositeExtract %float [[p1]] 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_B0_0 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] [[x]]
// CHECK-NEXT: [[y:%[0-9]+]] = OpCompositeExtract %float [[p1]] 1
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_B1_0 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] [[y]]
  prims[0] = attrs;

  tris[0] = uint3(0, 1, 2);
}
