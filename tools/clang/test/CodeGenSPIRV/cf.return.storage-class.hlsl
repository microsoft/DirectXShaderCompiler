// Run: %dxc -T ps_6_0 -E main

struct BufferType {
    float     a;
    float3    b;
    float3x2  c;
};

RWStructuredBuffer<BufferType> sbuf;  // %BufferType

// CHECK: %retSBuffer5 = OpFunction %BufferType_0 None {{%\d+}}
BufferType retSBuffer5() {            // BufferType_0
// CHECK:    %temp_var_ret = OpVariable %_ptr_Function_BufferType_0 Function

// CHECK-NEXT: [[sbuf:%\d+]] = OpAccessChain %_ptr_Uniform_BufferType %sbuf %int_0 %uint_5
// CHECK-NEXT:  [[val:%\d+]] = OpLoad %BufferType [[sbuf]]
// CHECK-NEXT:    [[a:%\d+]] = OpCompositeExtract %float [[val]] 0
// CHECK-NEXT:    [[b:%\d+]] = OpCompositeExtract %v3float [[val]] 1
// CHECK-NEXT:    [[c:%\d+]] = OpCompositeExtract %mat3v2float [[val]] 2
// CHECK-NEXT:  [[tmp:%\d+]] = OpCompositeConstruct %BufferType_0 [[a]] [[b]] [[c]]
// CHECK-NEXT:                 OpStore %temp_var_ret [[tmp]]
// CHECK-NEXT:  [[tmp:%\d+]] = OpLoad %BufferType_0 %temp_var_ret
// CHECK-NEXT:       OpReturnValue [[tmp]]
// CHECK-NEXT:       OpFunctionEnd
    return sbuf[5];
}

void main() {
    sbuf[6] = retSBuffer5();
}
