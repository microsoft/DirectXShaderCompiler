// RUN: %dxc -T ps_6_0 -E main

struct PSInput
{
    float2 foo[4] : SV_ClipDistance0;
    float3 bar[2] : SV_ClipDistance1;
};

float4 main(PSInput input) : SV_TARGET
{
// CHECK:  [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_0
// CHECK: [[load0:%\d+]] = OpLoad %float [[ptr0]]

// CHECK:  [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_1
// CHECK: [[load1:%\d+]] = OpLoad %float [[ptr1]]

// CHECK: [[vec0:%\d+]] = OpCompositeConstruct %v2float [[load0]] [[load1]]

// CHECK:  [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_2
// CHECK: [[load2:%\d+]] = OpLoad %float [[ptr2]]

// CHECK:  [[ptr3:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_3
// CHECK: [[load3:%\d+]] = OpLoad %float [[ptr3]]

// CHECK: [[vec1:%\d+]] = OpCompositeConstruct %v2float [[load2]] [[load3]]

// CHECK:  [[ptr4:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_4
// CHECK: [[load4:%\d+]] = OpLoad %float [[ptr4]]

// CHECK:  [[ptr5:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_5
// CHECK: [[load5:%\d+]] = OpLoad %float [[ptr5]]

// CHECK: [[vec2:%\d+]] = OpCompositeConstruct %v2float [[load4]] [[load5]]

// CHECK:  [[ptr6:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_6
// CHECK: [[load6:%\d+]] = OpLoad %float [[ptr6]]

// CHECK:  [[ptr7:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_7
// CHECK: [[load7:%\d+]] = OpLoad %float [[ptr7]]

// CHECK:   [[vec3:%\d+]] = OpCompositeConstruct %v2float [[load6]] [[load7]]
// CHECK: [[array0:%\d+]] = OpCompositeConstruct %_arr_v2float_uint_4 [[vec0]] [[vec1]] [[vec2]] [[vec3]]

// CHECK:  [[ptr8:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_8
// CHECK: [[load8:%\d+]] = OpLoad %float [[ptr8]]

// CHECK:  [[ptr9:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_9
// CHECK: [[load9:%\d+]] = OpLoad %float [[ptr9]]

// CHECK:  [[ptr10:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_10
// CHECK: [[load10:%\d+]] = OpLoad %float [[ptr10]]

// CHECK: [[vec0:%\d+]] = OpCompositeConstruct %v3float [[load8]] [[load9]] [[load10]]

// CHECK:  [[ptr11:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_11
// CHECK: [[load11:%\d+]] = OpLoad %float [[ptr11]]

// CHECK:  [[ptr12:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_12
// CHECK: [[load12:%\d+]] = OpLoad %float [[ptr12]]

// CHECK:  [[ptr13:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_13
// CHECK: [[load13:%\d+]] = OpLoad %float [[ptr13]]

// CHECK:   [[vec1:%\d+]] = OpCompositeConstruct %v3float [[load11]] [[load12]] [[load13]]
// CHECK: [[array1:%\d+]] = OpCompositeConstruct %_arr_v3float_uint_2 [[vec0]] [[vec1]]

// CHECK:                   OpCompositeConstruct %PSInput [[array0]] [[array1]]

    float4 result = {input.foo[0], input.bar[1].yz};
    return result;
}
