// RUN: %dxc -T ps_6_0 -E main

struct PSInput
{
    float4 color[2] : SV_ClipDistance;
};

float4 main(PSInput input) : SV_TARGET
{
// CHECK:  [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_0
// CHECK: [[load0:%\d+]] = OpLoad %float [[ptr0]]

// CHECK:  [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_1
// CHECK: [[load1:%\d+]] = OpLoad %float [[ptr1]]

// CHECK:  [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_2
// CHECK: [[load2:%\d+]] = OpLoad %float [[ptr2]]

// CHECK:  [[ptr3:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_3
// CHECK: [[load3:%\d+]] = OpLoad %float [[ptr3]]

// CHECK: [[vec0:%\d+]] = OpCompositeConstruct %v4float [[load0]] [[load1]] [[load2]] [[load3]]

// CHECK:  [[ptr4:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_4
// CHECK: [[load4:%\d+]] = OpLoad %float [[ptr4]]

// CHECK:  [[ptr5:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_5
// CHECK: [[load5:%\d+]] = OpLoad %float [[ptr5]]

// CHECK:  [[ptr6:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_6
// CHECK: [[load6:%\d+]] = OpLoad %float [[ptr6]]

// CHECK:  [[ptr7:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_7
// CHECK: [[load7:%\d+]] = OpLoad %float [[ptr7]]

// CHECK: [[vec1:%\d+]] = OpCompositeConstruct %v4float [[load4]] [[load5]] [[load6]] [[load7]]

// CHECK: [[array:%\d+]] = OpCompositeConstruct %_arr_v4float_uint_2 [[vec0]] [[vec1]]
// CHECK:                  OpCompositeConstruct %PSInput [[array]]

    return input.color[0];
}
