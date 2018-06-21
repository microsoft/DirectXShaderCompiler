// Run: %dxc -T ps_6_0 -E main

struct Basic {
    float3 a;
    float4 b;
};

// CHECK: %S = OpTypeStruct %_ptr_Uniform_type_AppendStructuredBuffer_v4float %_ptr_Uniform_type_ConsumeStructuredBuffer_v4float
struct S {
     AppendStructuredBuffer<float4> append;
    ConsumeStructuredBuffer<float4> consume;
};

// CHECK: %T = OpTypeStruct %_ptr_Uniform_type_StructuredBuffer_Basic %_ptr_Uniform_type_RWStructuredBuffer_Basic
struct T {
      StructuredBuffer<Basic> ro;
    RWStructuredBuffer<Basic> rw;

};

struct U {
    StructuredBuffer<Basic> basic;
    StructuredBuffer<int>   integer;

    StructuredBuffer<Basic> getSBufferStruct() { return basic;   }
    StructuredBuffer<int>   getSBufferInt()    { return integer; }
};

// CHECK: %Combine = OpTypeStruct %S %T %_ptr_Uniform_type_ByteAddressBuffer %_ptr_Uniform_type_RWByteAddressBuffer %U
struct Combine {
                      S s;
                      T t;
      ByteAddressBuffer ro;
    RWByteAddressBuffer rw;
                      U u;

    U getU() { return u; }
};

       StructuredBuffer<Basic>  gSBuffer;
     RWStructuredBuffer<Basic>  gRWSBuffer;
 AppendStructuredBuffer<float4> gASBuffer;
ConsumeStructuredBuffer<float4> gCSBuffer;
      ByteAddressBuffer         gBABuffer;
    RWByteAddressBuffer         gRWBABuffer;

float4 foo(Combine comb);

float4 main() : SV_Target {
    Combine c;

// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_AppendStructuredBuffer_v4float %c %int_0 %int_0
// CHECK-NEXT:                OpStore [[ptr]] %gASBuffer
    c.s.append = gASBuffer;
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_ConsumeStructuredBuffer_v4float %c %int_0 %int_1
// CHECK-NEXT:                OpStore [[ptr]] %gCSBuffer
    c.s.consume = gCSBuffer;

    T t;
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_StructuredBuffer_Basic %t %int_0
// CHECK-NEXT:                OpStore [[ptr]] %gSBuffer
    t.ro = gSBuffer;
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_RWStructuredBuffer_Basic %t %int_1
// CHECK-NEXT:                OpStore [[ptr]] %gRWSBuffer
    t.rw = gRWSBuffer;
// CHECK:      [[val:%\d+]] = OpLoad %T %t
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_T %c %int_1
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    c.t = t;

// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_ByteAddressBuffer %c %int_2
// CHECK-NEXT:                OpStore [[ptr]] %gBABuffer
    c.ro = gBABuffer;
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_RWByteAddressBuffer %c %int_3
// CHECK-NEXT:                OpStore [[ptr]] %gRWBABuffer
    c.rw = gRWBABuffer;

// Make sure that we create temporary variable for intermediate objects since
// the function expect pointers as parameters.
// CHECK:     [[call1:%\d+]] = OpFunctionCall %U %Combine_getU %c
// CHECK-NEXT:                 OpStore %temp_var_U [[call1]]
// CHECK-NEXT:[[call2:%\d+]] = OpFunctionCall %_ptr_Uniform_type_StructuredBuffer_Basic %U_getSBufferStruct %temp_var_U
// CHECK-NEXT:  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_v4float [[call2]] %int_0 %uint_10 %int_1
// CHECK-NEXT:      {{%\d+}} = OpLoad %v4float [[ptr]]
    float4 val = c.getU().getSBufferStruct()[10].b;

// Check StructuredBuffer of scalar type
// CHECK:     [[call2:%\d+]] = OpFunctionCall %_ptr_Uniform_type_StructuredBuffer_int %U_getSBufferInt %temp_var_U_0
// CHECK-NEXT:      {{%\d+}} = OpAccessChain %_ptr_Uniform_int [[call2]] %int_0 %uint_42
    int index = c.getU().getSBufferInt()[42];

// CHECK:      [[val:%\d+]] = OpLoad %Combine %c
// CHECK:                     OpStore %param_var_comb [[val]]
    return foo(c);
}
float4 foo(Combine comb) {
// CHECK:      [[ptr1:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_ByteAddressBuffer %comb %int_2
// CHECK-NEXT: [[ptr2:%\d+]] = OpLoad %_ptr_Uniform_type_ByteAddressBuffer [[ptr1]]
// CHECK-NEXT:  [[idx:%\d+]] = OpShiftRightLogical %uint %uint_5 %uint_2
// CHECK-NEXT:      {{%\d+}} = OpAccessChain %_ptr_Uniform_uint [[ptr2]] %uint_0 [[idx]]
    uint val = comb.ro.Load(5);

// CHECK:      [[ptr1:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_StructuredBuffer_Basic %comb %int_1 %int_0
// CHECK-NEXT: [[ptr2:%\d+]] = OpLoad %_ptr_Uniform_type_StructuredBuffer_Basic [[ptr1]]
// CHECK-NEXT:      {{%\d+}} = OpAccessChain %_ptr_Uniform_v4float [[ptr2]] %int_0 %uint_0 %int_1
    return comb.t.ro[0].b;
}
