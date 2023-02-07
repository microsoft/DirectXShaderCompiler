// RUN: %dxc -T lib_6_6 %s | FileCheck %s

// Make sure vector array param works.
// CHECK:%[[Cast:.+]] = bitcast [3 x [3 x float]]* %0 to [3 x <3 x float>]*
// CHECK:call float @"\01?foo@@YAMY02V?$vector@M$02@@@Z"([3 x <3 x float>]* %[[Cast]]

float foo(float3 a[3]);

export
float bar(float3 a[3]) {
  return foo(a);
}
