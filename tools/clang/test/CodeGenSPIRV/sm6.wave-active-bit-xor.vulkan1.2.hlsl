// Run: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.2

// Note: WaveActiveBitXor() only accepts unsigned interger scalars/vectors.

// CHECK: ; Version: 1.5

struct S {
    uint4 val1;
    uint3 val2;
    uint2 val3;
     uint val4;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformArithmetic

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;
    uint4 val1 = values[x].val1;
    uint3 val2 = values[x].val2;
    uint2 val3 = values[x].val3;
     uint val4 = values[x].val4;

// CHECK:      [[val1:%\d+]] = OpLoad %v4uint %val1
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformBitwiseXor %v4uint %uint_3 Reduce [[val1]]
    values[x].val1 = WaveActiveBitXor(val1);
// CHECK:      [[val2:%\d+]] = OpLoad %v3uint %val2
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformBitwiseXor %v3uint %uint_3 Reduce [[val2]]
    values[x].val2 = WaveActiveBitXor(val2);
// CHECK:      [[val3:%\d+]] = OpLoad %v2uint %val3
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformBitwiseXor %v2uint %uint_3 Reduce [[val3]]
    values[x].val3 = WaveActiveBitXor(val3);
// CHECK:      [[val4:%\d+]] = OpLoad %uint %val4
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformBitwiseXor %uint %uint_3 Reduce [[val4]]
    values[x].val4 = WaveActiveBitXor(val4);
}
