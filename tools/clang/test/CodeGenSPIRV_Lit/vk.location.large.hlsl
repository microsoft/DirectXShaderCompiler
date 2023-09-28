// RUN: not %dxc -T vs_6_0 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s

[[vk::location(123456)]]
float main() : A { return 1.0; }

// CHECK: 3:3: error: stage output location #123456 too large
