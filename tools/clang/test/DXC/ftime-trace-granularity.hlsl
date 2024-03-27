// RUN: %dxc -E main -T vs_6_0 %s -ftime-trace -ftime-trace-granularity=99999 | FileCheck %s
// RUN: %dxc -E main -T vs_6_0 %s -ftime-trace=%t.json -ftime-trace-granularity=99999
// RUN: cat %t.json | FileCheck %s

// This test runs both stdout and file output paths.
// Validate that we do not output named stats
// but still continue to output the Totals
// versions of them which is expected.
// CHECK: { "traceEvents": [
// CHECK-NOT: "name":"Frontend"
// CHECK: "name":"Total Frontend"

void main() {}
