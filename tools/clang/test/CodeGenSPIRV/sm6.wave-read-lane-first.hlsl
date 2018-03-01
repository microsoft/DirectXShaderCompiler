// Run: %dxc -T cs_6_0 -E main

// CHECK: OpCapability SubgroupBallotKHR
// CHECK: OpExtension "SPV_KHR_shader_ballot"

struct S {
    uint4 val1;
     int2 val2;
    float val3;
};

RWStructuredBuffer<S> values;

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;

    uint4 val1 = values[x].val1;
     int2 val2 = values[x].val2;
    float val3 = values[x].val3;

// OpSubgroupFirstInvocationKHR requires that:
//   Result Type must be a 32-bit integer type or a 32-bit float type scalar.

    // values[x].val1 = WaveReadLaneFirst(val1);
    // values[x].val2 = WaveReadLaneFirst(val2);
// CHECK:      [[val3:%\d+]] = OpLoad %float %val3
// CHECK-NEXT:      {{%\d+}} = OpSubgroupFirstInvocationKHR %float [[val3]]
    values[x].val3 = WaveReadLaneFirst(val3);
}
