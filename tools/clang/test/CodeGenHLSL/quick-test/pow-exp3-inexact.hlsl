// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: Log
// CHECK: Exp

float main ( float a : A) : SV_Target
{
    return pow(a, 3.0001);
}