// Run: %dxc -T ps_6_0 -E main

struct Basic {
    float3 a;
    float4 b;
};

// CHECK: %S = OpTypeStruct %_ptr_Uniform_type_AppendStructuredBuffer_v4float %_ptr_Uniform_type_AppendStructuredBuffer_v4float
struct S {
     AppendStructuredBuffer<float4> append;
    ConsumeStructuredBuffer<float4> consume;
};

// CHECK: %T = OpTypeStruct %_ptr_Uniform_type_StructuredBuffer_Basic %_ptr_Uniform_type_RWStructuredBuffer_Basic %_ptr_Uniform_type_StructuredBuffer_int
struct T {
      StructuredBuffer<Basic> ro;
    RWStructuredBuffer<Basic> rw;
      StructuredBuffer<int>   ro2;

    StructuredBuffer<Basic> getSBuffer() { return ro; }
    StructuredBuffer<int>   getSBuffer2() { return ro2; }
};

// CHECK: %Combine = OpTypeStruct %S %T %_ptr_Uniform_type_ByteAddressBuffer %_ptr_Uniform_type_RWByteAddressBuffer
struct Combine {
                      S s;
                      T t;
      ByteAddressBuffer ro;
    RWByteAddressBuffer rw;

    T getT() { return t; }
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
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_AppendStructuredBuffer_v4float %c %int_0 %int_1
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
// CHECK:     [[call1:%\d+]] = OpFunctionCall %T %Combine_getT %c
// CHECK-NEXT:                 OpStore %temp_var_T [[call1]]
// CHECK-NEXT:[[call2:%\d+]] = OpFunctionCall %_ptr_Uniform_type_StructuredBuffer_Basic %T_getSBuffer %temp_var_T
// CHECK-NEXT:  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_v4float [[call2]] %int_0 %uint_10 %int_1
// CHECK-NEXT:      {{%\d+}} = OpLoad %v4float [[ptr]]
    float4 val = c.getT().getSBuffer()[10].b;

// Check StructuredBuffer of scalar type
// CHECK:     [[call2:%\d+]] = OpFunctionCall %_ptr_Uniform_type_StructuredBuffer_int %T_getSBuffer2 %temp_var_T_0
// CHECK-NEXT:      {{%\d+}} = OpAccessChain %_ptr_Uniform_int [[call2]] %int_0 %uint_42
    int index = c.getT().getSBuffer2()[42];

// CHECK:      [[val:%\d+]] = OpLoad %Combine %c
// CHECK-NEXT:                OpStore %param_var_comb [[val]]
    return foo(c);
}
float4 foo(Combine comb) {
    // TODO: add support for associated counters of struct fields
    // comb.s.append.Append(float4(1, 2, 3, 4));
    // float4 val = comb.s.consume.Consume();
    // comb.t.rw[5].a = 4.2;

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
