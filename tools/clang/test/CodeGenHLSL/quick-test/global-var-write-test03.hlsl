// RUN: %dxc /Od -T ps_6_0 -E PSMain /Gec -HV 2016 > %s | FileCheck %s

// CHECK: {{.*cbvar.*}} = constant float 0.000000e+00, align 4
// CHECK: {{(.*cbvar.*)(.*.static.copy.*)}} = internal global float 0.000000e+00
// CHECK: define void @PSMain()
// CHECK: %{{[a-z0-9]+}} = load float, float* {{(.*cbvar.*)(.*.static.copy.*)}}, align 4
// CHECK: %{{[a-z0-9]+}} = fadd fast float %{{[a-z0-9]+}}, 5.000000e+00
// CHECK: store float %{{[a-z0-9]+}}, float* {{(.*cbvar.*)(.*.static.copy.*)}}, align 4
// CHECK: %{{[a-z0-9]+}} = load float, float* {{(.*cbvar.*)(.*.static.copy.*)}}, align 4
// CHECK: %{{[a-z0-9]+}} = fmul fast float %{{[a-z0-9]+}}, 3.000000e+00
// CHECK: store float %{{[a-z0-9]+}}, float* {{(.*cbvar.*)(.*.static.copy.*)}}, align 4
// CHECK: ret void

float cbvar;

void AddVal() { cbvar += 5.0; }
void MulVal() { cbvar *= 3.0; }
float GetVal() { return cbvar; }

float PSMain() : SV_Target
{
  AddVal();
  MulVal();
  return GetVal();
}