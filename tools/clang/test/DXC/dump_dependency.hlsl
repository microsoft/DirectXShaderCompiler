// RUN: %dxc /T ps_6_0 %s -I %S/Inputs /M | FileCheck %s

// RUN: %dxc /T ps_6_0 %s -I %S/Inputs /MF%t.deps
// RUN: FileCheck --input-file=%t.deps %s

// RUN: %dxc /T ps_6_0 %s -I %S/Inputs /MD
// RUN: FileCheck --input-file=%S/dump_dependency.d %s

// CHECK-DAG:{{^([a-zA-Z]\\:\\\\|/|\\\\)}}
// CHECK-DAG:dump_dependency.hlsl:{{[ ]}}
// CHECK-DAG:{{^([a-zA-Z]\\:\\\\|/|\\\\)}}
// CHECK-DAG:dependency0.h \
// CHECK-DAG:{{^([a-zA-Z]\\:\\\\|/|\\\\)}}
// CHECK-DAG:dependency1.h \
// CHECK-DAG:{{^([a-zA-Z]\\:\\\\|/|\\\\)}}
// CHECK-DAG:dependency2.h \
// CHECK-DAG:{{^([a-zA-Z]\\:\\\\|/|\\\\)}}
// CHECK-DAG:dependency3.h \
// CHECK-DAG:{{^([a-zA-Z]\\:\\\\|/|\\\\)}}
// CHECK-DAG:dependency4.h \
// CHECK-DAG:{{^([a-zA-Z]\\:\\\\|/|\\\\)}}
// CHECK-DAG:dependency5.h \
// CHECK-DAG:{{^([a-zA-Z]\\:\\\\|/|\\\\)}}
// CHECK-DAG:dependency6\ $$\#\[\].h
// CHECK-NOT:\

// RUN: rm %S/dump_dependency.d

#include "include/dependency0.h"
#include "include/dependency2.h"
#include "include/dependency6 $#[].h"

float4 main() : SV_Target
{
  return 0;
}
