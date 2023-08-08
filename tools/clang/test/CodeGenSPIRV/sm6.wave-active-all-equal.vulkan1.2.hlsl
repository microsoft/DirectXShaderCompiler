// RUN: %dxc -T cs_6_0 -E main -HV 2018 -fspv-target-env=vulkan1.2

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
// CHECK: [[element0:%\w+]] = OpCompositeExtract %float [[f32val]] 0
// CHECK: [[res0:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element0]]
// CHECK: [[element1:%\w+]] = OpCompositeExtract %float [[f32val]] 1
// CHECK: [[res1:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element1]]
// CHECK: [[element2:%\w+]] = OpCompositeExtract %float [[f32val]] 2
// CHECK: [[res2:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element2]]
// CHECK: [[element3:%\w+]] = OpCompositeExtract %float [[f32val]] 3
// CHECK: [[res3:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element3]]
// CHECK: [[vec_res:%\w+]] = OpCompositeConstruct %v4bool [[res0]] [[res1]] [[res2]] [[res3]]

// CHECK:         [[ptr:%\d+]] = OpAccessChain %_ptr_StorageBuffer_uint %values %int_0 {{%\d+}} %int_1
// CHECK-NEXT: [[u32val:%\d+]] = OpLoad %uint [[ptr]]
// CHECK-NEXT:        {{%\d+}} = OpGroupNonUniformAllEqual %bool %uint_3 [[u32val]]
    values[x].res = WaveActiveAllEqual(values[x].val1) && WaveActiveAllEqual(values[x].val2);
}
