// RUN: %dxc -T ps_6_0 -E main

// CHECK: %_runtimearr_v4float = OpTypeRuntimeArray %v4float

// CHECK: %type_RWStructuredBuffer_v4float = OpTypeStruct %_runtimearr_v4float
// CHECK: %_arr_type_RWStructuredBuffer_v4float_uint_8 = OpTypeArray %type_RWStructuredBuffer_v4float %uint_8

// CHECK:    %MyRWSBuffer = OpVariable %_ptr_Uniform__arr_type_RWStructuredBuffer_v4float_uint_8 Uniform
RWStructuredBuffer<float4>   MyRWSBuffer[8];

float4 main(uint index : INDEX) : SV_Target {
// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_type_RWStructuredBuffer_v4float %MyRWSBuffer {{%\d+}}
// CHECK:                OpStore %localSBuffer [[ptr]]
    RWStructuredBuffer<float4>   localSBuffer = MyRWSBuffer[index];

// CHECK: [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_RWStructuredBuffer_v4float %localSBuffer
// CHECK:                OpAccessChain %_ptr_Uniform_v4float [[ptr]] %int_0 {{%\d+}}
    return localSBuffer[index];
}
