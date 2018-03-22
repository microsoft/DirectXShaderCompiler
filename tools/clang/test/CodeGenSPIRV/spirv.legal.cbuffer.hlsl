// Run: %dxc -T ps_6_0 -E main

// %type_ConstantBuffer_S is the type for myCBuffer. With layout decoration.
// %S is the type for myASBuffer elements. With layout decoration.
// %S_0 is the type for function local variables. Without layout decoration.

// CHECK:     OpMemberDecorate %type_ConstantBuffer_S 0 Offset 0
// CHECK:     OpMemberDecorate %S 0 Offset 0
// CHECK-NOT: OpMemberDecorate %S_0 0 Offset 0

// CHECK:     %type_ConstantBuffer_S = OpTypeStruct %v4float
// CHECK:     %S = OpTypeStruct %v4float
// CHECK:     %S_0 = OpTypeStruct %v4float
struct S {
    float4 f;
};

// CHECK: %myCBuffer = OpVariable %_ptr_Uniform_type_ConstantBuffer_S Uniform
ConstantBuffer<S>         myCBuffer;
AppendStructuredBuffer<S> myASBuffer;

S retStuff();

float4 doStuff(S buffer) {
    return buffer.f;
}

float4 main(in float4 pos : SV_Position) : SV_Target
{
// Initializing a T with a ConstantBuffer<T> is a copy
// CHECK:      [[val:%\d+]] = OpLoad %type_ConstantBuffer_S %myCBuffer
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeExtract %v4float [[val]] 0
// CHECK-NEXT: [[tmp:%\d+]] = OpCompositeConstruct %S_0 [[vec]]
// CHECK-NEXT:                OpStore %buffer1 [[tmp]]
    S buffer1 = myCBuffer;

// Assigning a ConstantBuffer<T> to a T is a copy
// CHECK:      [[val:%\d+]] = OpLoad %type_ConstantBuffer_S %myCBuffer
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeExtract %v4float [[val]] 0
// CHECK-NEXT: [[tmp:%\d+]] = OpCompositeConstruct %S_0 [[vec]]
// CHECK-NEXT:                OpStore %buffer2 [[tmp]]
    S buffer2;
    buffer2 = myCBuffer;

// We have the same struct type here
// CHECK:      [[val:%\d+]] = OpFunctionCall %S_0 %retStuff
// CHECK-NEXT:                OpStore %buffer3 [[val]]
    S buffer3;
    buffer3 = retStuff();

// Write out each component recursively
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_S %myASBuffer %uint_0 {{%\d+}}
// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_ConstantBuffer_S %myCBuffer
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeExtract %v4float [[val]] 0
// CHECK-NEXT: [[tmp:%\d+]] = OpCompositeConstruct %S [[vec]]
// CHECK-NEXT:                OpStore [[ptr]] [[tmp]]
    myASBuffer.Append(myCBuffer);

// Passing a ConstantBuffer<T> to a T parameter is a copy
// CHECK:      [[val:%\d+]] = OpLoad %type_ConstantBuffer_S %myCBuffer
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeExtract %v4float [[val]] 0
// CHECK-NEXT: [[tmp:%\d+]] = OpCompositeConstruct %S_0 [[vec]]
// CHECK-NEXT:                OpStore %param_var_buffer [[tmp]]
    return doStuff(myCBuffer);
}

S retStuff() {
// Returning a ConstantBuffer<T> as a T is a copy
// CHECK:      [[val:%\d+]] = OpLoad %type_ConstantBuffer_S %myCBuffer
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeExtract %v4float [[val]] 0
// CHECK-NEXT: [[tmp:%\d+]] = OpCompositeConstruct %S_0 [[vec]]
// CHECK-NEXT:                OpStore %temp_var_ret [[tmp]]
// CHECK-NEXT: [[ret:%\d+]] = OpLoad %S_0 %temp_var_ret
// CHECK-NEXT:                OpReturnValue [[ret]]
    return myCBuffer;
}
