// RUN: %dxc -E main -enable-unions -HV 202x -T vs_6_2 %s | FileCheck %s

struct s0 {
  uint abc;
};

union s1 {
  s0 a;
  float b;
};

// CHECK: ret
s1 main() : OUT {
  s1 s;
  return s;
}
