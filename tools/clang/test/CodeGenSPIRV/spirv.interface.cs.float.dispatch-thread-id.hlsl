// Run: %dxc -T cs_6_0 -E main

// CHECK: %gl_GlobalInvocationID = OpVariable %_ptr_Input_v3int Input

// CHECK: %in_var_SV_DispatchThreadID = OpVariable %_ptr_Function_v2float Function

// CHECK:         [[load:%\d+]] = OpLoad %v3int %gl_GlobalInvocationID
// CHECK-NEXT: [[shuffle:%\d+]] = OpVectorShuffle %v2int [[load]] [[load]] 0 1
// CHECK-NEXT:    [[cast:%\d+]] = OpConvertSToF %v2float [[shuffle]]
// CHECK-NEXT:                    OpStore %in_var_SV_DispatchThreadID [[cast]]
// CHECK-NEXT:                    OpLoad %v2float %in_var_SV_DispatchThreadID

RWStructuredBuffer<float4> rwTexture;

[numthreads(1, 1, 1)]
void main(float2 id : SV_DispatchThreadID)
{
    rwTexture[3] = id.xxxx;
}
