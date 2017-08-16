// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: Low precision data types present

// CHECK: add i16
// CHECK: uitofp i16
// CHECK: to float

float main(min16uint a : A) : SV_Target
{
    return a - 3;
}
