// RUN: %dxc -E main -T ps_6_0 -M %s | FileCheck %s
// RUN: %dxc -E main -T ps_6_0 -MF%s.d %s && cat %s.d | FileCheck %s
// RUN: %dxc -E main -T ps_6_0 -MD %s && cat %S/dump_dependency.d | FileCheck %s

// CHECK:      dump_dependency.hlsl
// CHECK-SAME: dump_dependency.hlsl
// CHECK-DAG:  dependency0.h
// CHECK-DAG:  dependency1.h
// CHECK-DAG:  dependency2.h
// CHECK-DAG:  dependency3.h
// CHECK-DAG:  dependency4.h
// CHECK-DAG:  dependency5.h

#include "dependency0.h"
#include "dependency2.h"

float4 main() : SV_Target
{
  return 0;
}
