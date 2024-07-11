// RUN: %dxc /Tps_6_0 %s | FileCheck %s
// CHECK: error: SV_Target semantic index must be between 0 and 7

float4 main() : SV_Target8
{
    return float4(0, 1, 2, 3);
}
