// Run: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.1

// CHECK: ; Version: 1.3

RWStructuredBuffer<uint> values;

// CHECK: OpCapability GroupNonUniform

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
// CHECK: {{%\d+}} = OpGroupNonUniformElect %bool %uint_3
    values[id.x] = WaveIsFirstLane();
}
