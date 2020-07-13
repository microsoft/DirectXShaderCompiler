// Run: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.2

// CHECK: ; Version: 1.5

struct S {
    float4 val1;
    uint val2;
    bool res;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformVote

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;
// CHECK:         [[ptr:%\d+]] = OpAccessChain %_ptr_StorageBuffer_v4float %values %int_0 {{%\d+}} %int_0
// CHECK-NEXT: [[f32val:%\d+]] = OpLoad %v4float [[ptr]]
// TODO: The front end will return bool4 for the first call, which acutally should be bool.
// XXXXX-NEXT:        {{%\d+}} = OpGroupNonUniformAllEqual %bool %uint_3 [[f32val]]

// CHECK:         [[ptr:%\d+]] = OpAccessChain %_ptr_StorageBuffer_uint %values %int_0 {{%\d+}} %int_1
// CHECK-NEXT: [[u32val:%\d+]] = OpLoad %uint [[ptr]]
// CHECK-NEXT:        {{%\d+}} = OpGroupNonUniformAllEqual %bool %uint_3 [[u32val]]
    values[x].res = WaveActiveAllEqual(values[x].val1) && WaveActiveAllEqual(values[x].val2);
}
