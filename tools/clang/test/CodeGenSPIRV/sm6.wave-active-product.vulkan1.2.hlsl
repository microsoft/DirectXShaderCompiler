// RUN: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.2 -fcgl  %s -spirv | FileCheck %s

// CHECK: ; Version: 1.5

struct S {
    float4 val1;
     uint2 val2;
       int val3;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformArithmetic

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;
    float4 val1 = values[x].val1;
     uint2 val2 = values[x].val2;
       int val3 = values[x].val3;

// CHECK:      [[val1:%[0-9]+]] = OpLoad %v4float %val1
// CHECK-NEXT:      {{%[0-9]+}} = OpGroupNonUniformFMul %v4float %uint_3 Reduce [[val1]]
    values[x].val1 = WaveActiveProduct(val1);
// CHECK:      [[val2:%[0-9]+]] = OpLoad %v2uint %val2
// CHECK-NEXT:      {{%[0-9]+}} = OpGroupNonUniformIMul %v2uint %uint_3 Reduce [[val2]]
    values[x].val2 = WaveActiveProduct(val2);
// CHECK:      [[val3:%[0-9]+]] = OpLoad %int %val3
// CHECK-NEXT:      {{%[0-9]+}} = OpGroupNonUniformIMul %int %uint_3 Reduce [[val3]]
    values[x].val3 = WaveActiveProduct(val3);
}
