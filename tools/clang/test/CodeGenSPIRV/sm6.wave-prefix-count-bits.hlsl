// Run: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.1

// CHECK: ; Version: 1.3

struct S {
     uint val;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformBallot

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;

// CHECK:  {{%\d+}} = OpGroupNonUniformBallotBitCount %uint %int_3 ExclusiveScan {{%\d+}}
    values[x].val = WavePrefixCountBits(values[x].val == 0);
}
