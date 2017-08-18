// Run: %dxc -T vs_6_0 -E main

// CHECK:      OpName %type_ConstantBuffer_T "type.ConstantBuffer.T"
// CHECK-NEXT: OpMemberName %type_ConstantBuffer_T 0 "a"
// CHECK-NEXT: OpMemberName %type_ConstantBuffer_T 1 "b"
// CHECK-NEXT: OpMemberName %type_ConstantBuffer_T 2 "c"
// CHECK-NEXT: OpMemberName %type_ConstantBuffer_T 3 "d"
// CHECK-NEXT: OpMemberName %type_ConstantBuffer_T 4 "s"
// CHECK-NEXT: OpMemberName %type_ConstantBuffer_T 5 "t"

// CHECK:      OpName %MyCbuffer "MyCbuffer"
// CHECK:      OpName %AnotherCBuffer "AnotherCBuffer"
struct S {
    float  f1;
    float3 f2;
};

// CHECK: %type_ConstantBuffer_T = OpTypeStruct %bool %int %v2uint %mat3v4float %S %_arr_float_uint_4
// CHECK: %_ptr_Uniform_type_ConstantBuffer_T = OpTypePointer Uniform %type_ConstantBuffer_T
struct T {
    bool     a;
    int      b;
    uint2    c;
    float3x4 d;
    S        s;
    float    t[4];
};

// CHECK: %MyCbuffer = OpVariable %_ptr_Uniform_type_ConstantBuffer_T Uniform
ConstantBuffer<T> MyCbuffer : register(b1);
// CHECK: %AnotherCBuffer = OpVariable %_ptr_Uniform_type_ConstantBuffer_T Uniform
ConstantBuffer<T> AnotherCBuffer : register(b2);

void main() {
}
