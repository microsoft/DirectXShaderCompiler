// RUN: %dxc -T lib_6_9 -HV 2021 -verify %s
// RUN: %dxc -T lib_6_9 -HV 2021 -ast-dump %s | FileCheck %s

// expected-no-diagnostics

// CHECK: FunctionDecl {{.*}} f 'float (float, float)'
// CHECK-NEXT: ParmVarDecl {{.*}} x 'float'
// CHECK-NEXT: HLSLNoDiffAttr
// CHECK-NEXT: ParmVarDecl {{.*}} y 'float'

float f([[dxc::NoDiff]] float x, float y) {
  return x * y;
}
