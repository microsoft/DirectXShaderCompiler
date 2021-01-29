// Run: %dxc -T ps_6_0 -E main

struct StructA
{
    uint one;
};

// CHECK: OpDecorate %buffer1 Coherent
globallycoherent RWByteAddressBuffer buffer1;

// CHECK: OpDecorate %buffer2 Coherent
globallycoherent RWStructuredBuffer<StructA> buffer2;

// CHECK-NOT: OpDecorate %buffer3 Coherent
RWByteAddressBuffer buffer3;

// CHECK-NOT: OpDecorate %buffer4 Coherent
RWStructuredBuffer<StructA> buffer4;

void main()
{
}