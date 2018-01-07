// Run: %dxc -T ps_6_0 -E main

struct S {
    float4 f;
};

struct T1 {
    float3 f;
};

struct T2 {
    float2 f;
};

StructuredBuffer<S>         globalSBuffer;
RWStructuredBuffer<S>       globalRWSBuffer;
AppendStructuredBuffer<T1>  globalASBuffer;
ConsumeStructuredBuffer<T2> globalCSBuffer;
ByteAddressBuffer           globalBABuffer;
RWByteAddressBuffer         globalRWBABuffer;

float4 main() : SV_Target {
// CHECK: %localSBuffer = OpVariable %_ptr_Function__ptr_Uniform_type_StructuredBuffer_S Function
// CHECK: %localRWSBuffer = OpVariable %_ptr_Function__ptr_Uniform_type_RWStructuredBuffer_S Function
// CHECK: %localASBuffer = OpVariable %_ptr_Function__ptr_Uniform_type_AppendStructuredBuffer_T1 Function
// CHECK: %localCSBuffer = OpVariable %_ptr_Function__ptr_Uniform_type_ConsumeStructuredBuffer_T2 Function
// CHECK: %localBABuffer = OpVariable %_ptr_Function__ptr_Uniform_type_ByteAddressBuffer Function
// CHECK: %localRWBABuffer = OpVariable %_ptr_Function__ptr_Uniform_type_RWByteAddressBuffer Function

// CHECK: OpStore %localSBuffer %globalSBuffer
// CHECK: OpStore %localRWSBuffer %globalRWSBuffer
// CHECK: OpStore %localASBuffer %globalASBuffer
// CHECK: OpStore %localCSBuffer %globalCSBuffer
// CHECK: OpStore %localBABuffer %globalBABuffer
// CHECK: OpStore %localRWBABuffer %globalRWBABuffer
    StructuredBuffer<S>         localSBuffer    = globalSBuffer;
    RWStructuredBuffer<S>       localRWSBuffer  = globalRWSBuffer;
    AppendStructuredBuffer<T1>  localASBuffer   = globalASBuffer;
    ConsumeStructuredBuffer<T2> localCSBuffer   = globalCSBuffer;
    ByteAddressBuffer           localBABuffer   = globalBABuffer;
    RWByteAddressBuffer         localRWBABuffer = globalRWBABuffer;

    T1 t1 = {float3(1., 2., 3.)};
    T2 t2;
    uint numStructs, stride, counter;
    float4 val;

// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_StructuredBuffer_S %localSBuffer
// CHECK-NEXT:     {{%\d+}} = OpArrayLength %uint [[ptr]] 0
    localSBuffer.GetDimensions(numStructs, stride);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_StructuredBuffer_S %localSBuffer
// CHECK-NEXT: [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_v4float [[ptr1]] %int_0 %int_1 %int_0
// CHECK-NEXT:      {{%\d+}} = OpLoad %v4float [[ptr2]]
    val = localSBuffer.Load(1).f;
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_StructuredBuffer_S %localSBuffer
// CHECK-NEXT: [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_v4float [[ptr1]] %int_0 %uint_2 %int_0
// CHECK-NEXT:      {{%\d+}} = OpLoad %v4float [[ptr2]]
    val = localSBuffer[2].f;

// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_RWStructuredBuffer_S %localRWSBuffer
// CHECK-NEXT:     {{%\d+}} = OpArrayLength %uint [[ptr]] 0
    localRWSBuffer.GetDimensions(numStructs, stride);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWStructuredBuffer_S %localRWSBuffer
// CHECK-NEXT: [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_v4float [[ptr1]] %int_0 %int_3 %int_0
// CHECK-NEXT:      {{%\d+}} = OpLoad %v4float [[ptr2]]
    val = localRWSBuffer.Load(3).f;
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWStructuredBuffer_S %localRWSBuffer
// CHECK-NEXT: [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_v4float [[ptr1]] %int_0 %uint_4 %int_0
// CHECK-NEXT:                 OpStore [[ptr2]] {{%\d+}}
    localRWSBuffer[4].f = 42.;

// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_AppendStructuredBuffer_T1 %localASBuffer
// CHECK-NEXT:     {{%\d+}} = OpArrayLength %uint [[ptr]] 0
    localASBuffer.GetDimensions(numStructs, stride);

// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ConsumeStructuredBuffer_T2 %localCSBuffer
// CHECK-NEXT:     {{%\d+}} = OpArrayLength %uint [[ptr]] 0
    localCSBuffer.GetDimensions(numStructs, stride);

    uint  byte;
    uint2 byte2;
    uint3 byte3;
    uint4 byte4;
    uint  dim;

    uint dest, value, compare, origin;

// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ByteAddressBuffer %localBABuffer
// CHECK-NEXT:     {{%\d+}} = OpArrayLength %uint [[ptr]] 0
    localBABuffer.GetDimensions(dim);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_ByteAddressBuffer %localBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:      {{%\d+}} = OpLoad %uint [[ptr2]]
    byte  = localBABuffer.Load(4);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_ByteAddressBuffer %localBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:      {{%\d+}} = OpLoad %uint [[ptr2]]
    byte2 = localBABuffer.Load2(5);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_ByteAddressBuffer %localBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:      {{%\d+}} = OpLoad %uint [[ptr2]]
    byte3 = localBABuffer.Load3(6);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_ByteAddressBuffer %localBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:      {{%\d+}} = OpLoad %uint [[ptr2]]
    byte4 = localBABuffer.Load4(7);

// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK-NEXT:     {{%\d+}} = OpArrayLength %uint [[ptr]] 0
    localRWBABuffer.GetDimensions(dim);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:      {{%\d+}} = OpLoad %uint [[ptr2]]
    byte  = localRWBABuffer.Load(8);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:      {{%\d+}} = OpLoad %uint [[ptr2]]
    byte2 = localRWBABuffer.Load2(9);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:      {{%\d+}} = OpLoad %uint [[ptr2]]
    byte3 = localRWBABuffer.Load3(10);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:      {{%\d+}} = OpLoad %uint [[ptr2]]
    byte4 = localRWBABuffer.Load4(11);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:                 OpStore [[ptr2]] {{%\d+}}
    localRWBABuffer.Store(12, byte);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:                 OpStore [[ptr2]] {{%\d+}}
    localRWBABuffer.Store2(13, byte2);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:                 OpStore [[ptr2]] {{%\d+}}
    localRWBABuffer.Store3(14, byte3);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
// CHECK-NEXT:                 OpStore [[ptr2]] {{%\d+}}
    localRWBABuffer.Store4(15, byte4);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
    localRWBABuffer.InterlockedAdd(dest, value, origin);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
    localRWBABuffer.InterlockedAnd(dest, value, origin);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
    localRWBABuffer.InterlockedCompareExchange(dest, compare, value, origin);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
    localRWBABuffer.InterlockedCompareStore(dest, compare, value);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
    localRWBABuffer.InterlockedExchange(dest, value, origin);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
    localRWBABuffer.InterlockedMax(dest, value, origin);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
    localRWBABuffer.InterlockedMin(dest, value, origin);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
    localRWBABuffer.InterlockedOr(dest, value, origin);
// CHECK:      [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_RWByteAddressBuffer %localRWBABuffer
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[ptr1]] %uint_0 {{%\d+}}
    localRWBABuffer.InterlockedXor(dest, value, origin);

    return val;
}
