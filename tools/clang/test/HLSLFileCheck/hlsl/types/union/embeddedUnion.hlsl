// RUN: %dxc -E main -enable-unions -T vs_6_2 %s | FileCheck %s

union s0 {
  uint abc;
};

union s1 {
  s0 a;
  uint b;
};

// CHECK: ret
s1 main() : OUT {
  s1 s;
  return s;
}
