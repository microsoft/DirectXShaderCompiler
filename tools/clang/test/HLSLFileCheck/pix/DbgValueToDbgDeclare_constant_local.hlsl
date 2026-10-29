// RUN: %dxc -T cs_6_0 -Od -Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

RWByteAddressBuffer RawUAV : register(u0);

[numthreads(1, 1, 1)]
void main()
{
    float bias = 0.5;
    bool hitFlag = true;
    RawUAV.Store(0, asuint(bias + (hitFlag ? 2.0 : 3.0)));
}

// CHECK: %[[hitFlag:.*]] = alloca [1 x i32]
// CHECK: %[[bias:.*]] = alloca [1 x float]
// CHECK: %[[bias_gep:.*]] = getelementptr [1 x float], [1 x float]* %[[bias]], i32 0, i32 0
// CHECK: store float 5.000000e-01, float* %[[bias_gep]]
// CHECK: %[[hitFlag_gep:.*]] = getelementptr [1 x i32], [1 x i32]* %[[hitFlag]], i32 0, i32 0
// CHECK: store i32 1, i32* %[[hitFlag_gep]]
