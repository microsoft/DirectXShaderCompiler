// RUN: %dxc -E main -enable-unions -HV 202x -T vs_6_2 %s | FileCheck %s

union s1 {
  int1x1 a;
  float4x2 b;
};

// CHECK: ret
float4x2 main() : OUT {
  s1 s;
  return s.b;
}
