
// RUN: %dxc -E main -HV 2016 -T vs_6_2 %s | FileCheck %s -check-prefix=CHECK-2016
// RUN: %dxc -E main -HV 2017 -T vs_6_2 %s | FileCheck %s -check-prefix=CHECK-2017
// RUN: %dxc -E main -HV 2018 -T vs_6_2 %s | FileCheck %s -check-prefix=CHECK-2018
// RUN: %dxc -E main -HV 2021 -T vs_6_2 %s | FileCheck %s -check-prefix=CHECK-2021
// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s -check-prefix=CHECK-202x

// CHECK-2016: error: 'union' is a reserved keyword in HLSL
// CHECK-2017: error: 'union' is a reserved keyword in HLSL
// CHECK-2018: error: 'union' is a reserved keyword in HLSL
// CHECK-2021: error: 'union' is a reserved keyword in HLSL
union A {
  int a;
  float b;
};

float main() : OUT {
  A uA;
  // CHECK-202x: ret void
  return uA.b;
}
