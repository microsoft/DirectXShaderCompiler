// RUN: %dxc -E main -T vs_6_0 %s -ftime-trace | FileCheck %s
// RUN: %dxc -E main -T vs_6_0 %s -ftime-trace=%t.json && FileCheck %s --input-file=%t.json

// CHECK: { "traceEvents": [

void main() {}
