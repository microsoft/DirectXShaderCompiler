// RUN: %dxc -T cs_6_7 -E main -fcgl  %s -spirv | FileCheck %s

// An arrayed multisampled storage image needs ImageMSArray on top of
// StorageImageMultisample. Accessing one without it fails validation, so this
// file declares no non-arrayed type: the capability is module-global and a
// module holding both could not tell the two requirements apart.
// CHECK-DAG: OpCapability StorageImageMultisample
// CHECK-DAG: OpCapability ImageMSArray

// OpTypeImage operands: sampled-type Dim Depth Arrayed MS Sampled Format.
// CHECK: %type_2d_image_array = OpTypeImage %int 2D 2 1 1 2 Rgba32i
// CHECK: %_ptr_UniformConstant_type_2d_image_array = OpTypePointer UniformConstant %type_2d_image_array

// CHECK: %t1 = OpVariable %_ptr_UniformConstant_type_2d_image_array UniformConstant
RWTexture2DMSArray<int4> t1 : register(u1);

[numthreads(1, 1, 1)]
void main() {
  uint3 coord = uint3(1, 2, 3);
// CHECK: OpImageWrite {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} Sample %uint_0
  t1[coord] = int4(1, 2, 3, 4);
}
