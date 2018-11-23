// Run: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.1

// CHECK: ; Version: 1.3

struct S {
    uint val;
    bool res;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformVote

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;
// CHECK:      [[cmp:%\d+]] = OpIEqual %bool {{%\d+}} %uint_0
// CHECK-NEXT:     {{%\d+}} = OpGroupNonUniformAny %bool %uint_3 [[cmp]]
    values[x].res = WaveActiveAnyTrue(values[x].val == 0);
}
