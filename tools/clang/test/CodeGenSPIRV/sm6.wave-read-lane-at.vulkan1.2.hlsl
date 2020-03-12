// Run: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.2

// CHECK: ; Version: 1.5

struct S {
    float4 val1;
     uint3 val2;
       int val3;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformBallot

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;

    float4 val1 = values[x].val1;
     uint3 val2 = values[x].val2;
       int val3 = values[x].val3;

// CHECK:      [[val1:%\d+]] = OpLoad %v4float %val1
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformBroadcast %v4float %uint_3 [[val1]] %uint_15
    values[x].val1 = WaveReadLaneAt(val1, 15);
// CHECK:      [[val2:%\d+]] = OpLoad %v3uint %val2
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformBroadcast %v3uint %uint_3 [[val2]] %uint_42
    values[x].val2 = WaveReadLaneAt(val2, 42);
// CHECK:      [[val3:%\d+]] = OpLoad %int %val3
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformBroadcast %int %uint_3 [[val3]] %uint_15
    values[x].val3 = WaveReadLaneAt(val3, 15);
}
