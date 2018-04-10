// Run: %dxc -T ps_6_0 -E main

// Note: The following is invalid SPIR-V code.
//
// * The assignment ignores storage class (and thus layout) difference.

// %type_TextureBuffer_S is the type for myTBuffer. With layout decoration.
// %S is the type for myASBuffer elements. With layout decoration.
// %S_0 is the type for function local variables. Without layout decoration.

// CHECK:     OpMemberDecorate %type_TextureBuffer_S 0 Offset 0
// CHECK:     OpMemberDecorate %S 0 Offset 0
// CHECK-NOT: OpMemberDecorate %S_0 0 Offset 0

// CHECK:     %type_TextureBuffer_S = OpTypeStruct %v4float
// CHECK:     %S = OpTypeStruct %v4float
// CHECK:     %S_0 = OpTypeStruct %v4float
struct S {
    float4 f;
};

// CHECK: %myTBuffer = OpVariable %_ptr_Uniform_type_TextureBuffer_S Uniform
TextureBuffer<S>          myTBuffer;
AppendStructuredBuffer<S> myASBuffer;

S retStuff();

float4 doStuff(S buffer) {
    return buffer.f;
}

float4 main(in float4 pos : SV_Position) : SV_Target
{
// Initializing a T with a TextureBuffer<T> is a copy
// CHECK:      [[val:%\d+]] = OpLoad %type_TextureBuffer_S %myTBuffer
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeExtract %v4float [[val]] 0
// CHECK-NEXT: [[tmp:%\d+]] = OpCompositeConstruct %S_0 [[vec]]
// CHECK-NEXT:                OpStore %buffer1 [[tmp]]
    S buffer1 = myTBuffer;

// Assigning a TextureBuffer<T> to a T is a copy
// CHECK:      [[val:%\d+]] = OpLoad %type_TextureBuffer_S %myTBuffer
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeExtract %v4float [[val]] 0
// CHECK-NEXT: [[tmp:%\d+]] = OpCompositeConstruct %S_0 [[vec]]
// CHECK-NEXT:                OpStore %buffer2 [[tmp]]
    S buffer2;
    buffer2 = myTBuffer;

// We have the same struct type here
// CHECK:      [[val:%\d+]] = OpFunctionCall %S_0 %retStuff
// CHECK-NEXT:                OpStore %buffer3 [[val]]
    S buffer3;
    buffer3 = retStuff();

// TODO: The underlying struct type has the same layout but %type_TextureBuffer_S
// has an additional BufferBlock decoration. So this causes an validation error.
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_S %myASBuffer %uint_0 {{%\d+}}
// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_TextureBuffer_S %myTBuffer
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    myASBuffer.Append(myTBuffer);

// Passing a TextureBuffer<T> to a T parameter is a copy
// CHECK:      [[val:%\d+]] = OpLoad %type_TextureBuffer_S %myTBuffer
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeExtract %v4float [[val]] 0
// CHECK-NEXT: [[tmp:%\d+]] = OpCompositeConstruct %S_0 [[vec]]
// CHECK-NEXT:                OpStore %param_var_buffer [[tmp]]
    return doStuff(myTBuffer);
}

S retStuff() {
// Returning a TextureBuffer<T> as a T is a copy
// CHECK:      [[val:%\d+]] = OpLoad %type_TextureBuffer_S %myTBuffer
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeExtract %v4float [[val]] 0
// CHECK-NEXT: [[tmp:%\d+]] = OpCompositeConstruct %S_0 [[vec]]
// CHECK-NEXT:                OpStore %temp_var_ret [[tmp]]
// CHECK-NEXT: [[ret:%\d+]] = OpLoad %S_0 %temp_var_ret
// CHECK-NEXT:                OpReturnValue [[ret]]
    return myTBuffer;
}
