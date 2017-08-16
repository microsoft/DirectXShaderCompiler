// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: Low precision data types present
// CHECK: fpext half
// CHECK: to float

float main(min16float a : A) : SV_Target
{
  return a;
}
