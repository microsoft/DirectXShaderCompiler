// RUN: %dxc -T cs_6_9 -E main -HV 2021 -O3 %s | FileCheck %s --implicit-check-not=i128
// RUN: %dxc -T cs_6_10 -E main -HV 2021 -O3 %s | FileCheck %s --implicit-check-not=i128

// A loop-carried float4 store forwarded to a uint4 load must not introduce
// an i128 temporary. Check both successful DXIL validation and vector codegen.
// CHECK: define void @main()
// CHECK-DAG: fadd{{.*}} <4 x float>
// CHECK-DAG: ret void

ByteAddressBuffer input : register(t0);
RWByteAddressBuffer output : register(u1);
cbuffer Params : register(b2) { uint rounds; }
groupshared uint4 sums[64];

[numthreads(64, 1, 1)]
void main(uint lane : SV_GroupIndex) {
    sums[lane] = uint4(0, 0, 0, 0);
    for (uint i = 0; i < rounds; ++i) {
        sums[lane] = asuint(asfloat(sums[lane]) +
                            asfloat(input.Load4((i * 64 + lane) * 16)));
    }
    GroupMemoryBarrierWithGroupSync();
    output.Store4(lane * 16, sums[lane]);
}
