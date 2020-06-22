// Run: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.2

// CHECK: ; Version: 1.5

struct S {
     uint4 val1;
    float2 val2;
       int val3;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformArithmetic

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;
     uint4 val1 = values[x].val1;
    float2 val2 = values[x].val2;
       int val3 = values[x].val3;

// CHECK:      [[val1:%\d+]] = OpLoad %v4uint %val1
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformUMin %v4uint %uint_3 Reduce [[val1]]
    values[x].val1 = WaveActiveMin(val1);
// CHECK:      [[val2:%\d+]] = OpLoad %v2float %val2
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformFMin %v2float %uint_3 Reduce [[val2]]
    values[x].val2 = WaveActiveMin(val2);
// CHECK:      [[val3:%\d+]] = OpLoad %int %val3
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformSMin %int %uint_3 Reduce [[val3]]
    values[x].val3 = WaveActiveMin(val3);
}
