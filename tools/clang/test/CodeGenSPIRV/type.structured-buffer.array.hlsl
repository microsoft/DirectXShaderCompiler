// Run: %dxc -T ps_6_0 -E main

// CHECK: %_runtimearr_v4float = OpTypeRuntimeArray %v4float

// CHECK: %type_StructuredBuffer_v4float = OpTypeStruct %_runtimearr_v4float
// CHECK: %_arr_type_StructuredBuffer_v4float_uint_8 = OpTypeArray %type_StructuredBuffer_v4float %uint_8

// CHECK: %_runtimearr_uint = OpTypeRuntimeArray %uint

// CHECK: %type_ByteAddressBuffer = OpTypeStruct %_runtimearr_uint
// CHECK: %_runtimearr_type_ByteAddressBuffer = OpTypeRuntimeArray %type_ByteAddressBuffer

// CHECK: %type_RWByteAddressBuffer = OpTypeStruct %_runtimearr_uint
// CHECK: %_arr_type_RWByteAddressBuffer_uint_4 = OpTypeArray %type_RWByteAddressBuffer %uint_4

// CHECK:    %MySBuffer = OpVariable %_ptr_Uniform__arr_type_StructuredBuffer_v4float_uint_8 Uniform
StructuredBuffer<float4>   MySBuffer[8];
// CHECK:   %MyBABuffer = OpVariable %_ptr_Uniform__runtimearr_type_ByteAddressBuffer Uniform
ByteAddressBuffer          MyBABuffer[];
// CHECK: %MyRWBABuffer = OpVariable %_ptr_Uniform__arr_type_RWByteAddressBuffer_uint_4 Uniform
RWByteAddressBuffer        MyRWBABuffer[4];

float4 main(uint index : INDEX) : SV_Target {
// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_type_ByteAddressBuffer %MyBABuffer {{%\d+}}
// CHECK:                OpAccessChain %_ptr_Uniform_uint [[ptr]] %uint_0 {{%\d+}}
    uint val1 = MyBABuffer[index].Load(index);

// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_type_RWByteAddressBuffer %MyRWBABuffer {{%\d+}}
// CHECK:                OpAccessChain %_ptr_Uniform_uint [[ptr]] %uint_0 {{%\d+}}
    MyRWBABuffer[index].Store(index, val1);

// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_type_StructuredBuffer_v4float %MySBuffer {{%\d+}}
// CHECK:                OpStore %localSBuffer [[ptr]]
    StructuredBuffer<float4>   localSBuffer = MySBuffer[index];

// CHECK: [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_StructuredBuffer_v4float %localSBuffer
// CHECK:                OpAccessChain %_ptr_Uniform_v4float [[ptr]] %int_0 {{%\d+}}
    float4 val2 = localSBuffer[index];

    return val1 * val2;
}
