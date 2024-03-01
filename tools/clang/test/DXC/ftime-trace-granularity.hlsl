// RUN: %dxc -E main -T vs_6_0 %s -ftime-trace | FileCheck %s
// RUN: %dxc -E main -T vs_6_0 %s -ftime-trace=%t.json -ftime-trace-granularity=0
// RUN: cat %t.json | FileCheck %s

// CHECK: { "traceEvents": [

void main() {}
