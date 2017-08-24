// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-force-early-z | %FileCheck %s

// Just check that the last line (which contains global flags) has the "8" meaning force-early-z:
// CHECK: !{i32 0, i64 8}
// Check there are no more entries (i.e. the above really was the last line)
// CHECK-NOT: !{

[RootSignature("")]
float4 main() : SV_Target {
    return float4(0,0,0,0);
}