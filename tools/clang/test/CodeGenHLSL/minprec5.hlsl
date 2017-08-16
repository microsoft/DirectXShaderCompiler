// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: Low precision data types present

// CHECK: fptrunc float
// CHECK: to half
// CHECK: fadd fast half

float main(float a : A) : SV_Target
{
    return (min16float)a + (min16float)5.f;
}
