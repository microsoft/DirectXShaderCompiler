// RUN: %dxc -T vs_6_0 -E main

struct VS_OUTPUT {
    float4 clips[2] : SV_ClipDistance;
};

// CHECK:       %VS_OUTPUT = OpTypeStruct %_arr_v4float_uint_2
// CHECK: %gl_ClipDistance = OpVariable %_ptr_Output__arr_float_uint_8 Output

VS_OUTPUT main() {
// CHECK: [[VS_OUTPUT:%\d+]] = OpFunctionCall %VS_OUTPUT %src_main

// CHECK:  [[clips:%\d+]] = OpCompositeExtract %_arr_v4float_uint_2 [[VS_OUTPUT]] 0
// CHECK: [[offset_base:%\d+]] = OpIAdd %uint %uint_0 %uint_0

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_0
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[clips]] 0 0
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[clips]] 0 1
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_2
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[clips]] 0 2
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_3
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[clips]] 0 3
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset_base:%\d+]] = OpIAdd %uint %uint_0 %uint_4

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_0
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[clips]] 1 0
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[clips]] 1 1
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_2
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[clips]] 1 2
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_3
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[clips]] 1 3
// CHECK:                   OpStore [[ptr]] [[value]]
    return (VS_OUTPUT) 0;
}
