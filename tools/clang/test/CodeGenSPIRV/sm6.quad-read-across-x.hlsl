// Run: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.1

// CHECK: ; Version: 1.3

struct S {
     int4 val1;
    uint3 val2;
    float val3;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformQuad

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;

     int4 val1 = values[x].val1;
    uint3 val2 = values[x].val2;
    float val3 = values[x].val3;

// CHECK:      [[val1:%\d+]] = OpLoad %v4int %val1
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformQuadSwap %v4int %uint_3 [[val1]] %uint_0
    values[x].val1 = QuadReadAcrossX(val1);
// CHECK:      [[val2:%\d+]] = OpLoad %v3uint %val2
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformQuadSwap %v3uint %uint_3 [[val2]] %uint_0
    values[x].val2 = QuadReadAcrossX(val2);
// CHECK:      [[val3:%\d+]] = OpLoad %float %val3
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformQuadSwap %float %uint_3 [[val3]] %uint_0
    values[x].val3 = QuadReadAcrossX(val3);
}
