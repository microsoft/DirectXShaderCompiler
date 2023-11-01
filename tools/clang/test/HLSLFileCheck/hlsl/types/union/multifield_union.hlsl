// RUN: %dxc -E main -enable-unions -HV 202x -T vs_6_2 %s | FileCheck %s

union s1 {
  bool a;
  uint b;
  float c;
};

// CHECK: ret
s1 main() : OUT {
  s1 s;
  return s;
}
