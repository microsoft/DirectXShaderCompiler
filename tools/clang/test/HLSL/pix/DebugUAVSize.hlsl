// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-debug-instrumentation,UAVSize=100000 | %FileCheck %s

// Check that the UAV size is reflected in the instrumentation. (Should be passed-in size - 64k)
// (The offset here is the "dumping ground" for non-interesting invocations)
// 100,000 - 65.536 = 34,464

// CHECK: %OffsetAddend = mul i32 34464, %ComplementOfMultiplicand


[RootSignature("")]
float4 main() : SV_Target {
    return float4(0,0,0,0);
}