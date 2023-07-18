// RUN: %dxc -T cs_6_0 -HV 2018 -E main -fspv-target-env=vulkan1.1

struct S {
    float4 val;
    bool res;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformVote

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {

// Each element of the vector must be extracted, and be passed to OpGroupNonUniformAllEqual. 
// CHECK: [[ld:%\w+]] = OpLoad %v4float
// CHECK: [[element0:%\w+]] = OpCompositeExtract %float [[ld]] 0
// CHECK: [[res0:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element0]]
// CHECK: [[element1:%\w+]] = OpCompositeExtract %float [[ld]] 1
// CHECK: [[res1:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element1]]
// CHECK: [[element2:%\w+]] = OpCompositeExtract %float [[ld]] 2
// CHECK: [[res2:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element2]]
// CHECK: [[element3:%\w+]] = OpCompositeExtract %float [[ld]] 3
// CHECK: [[res3:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element3]]

// Then the results must be combined into a boolean vector.
// CHECK: [[vec_res:%\w+]] = OpCompositeConstruct %v4bool [[res0]] [[res1]] [[res2]] [[res3]]
// CHECK: [[res:%\w+]] = OpAll %bool [[vec_res]]
// CHECK: OpSelect %uint [[res]] %uint_1 %uint_0 
    values[id.x].res = all(WaveActiveAllEqual(values[id.x].val));
}
