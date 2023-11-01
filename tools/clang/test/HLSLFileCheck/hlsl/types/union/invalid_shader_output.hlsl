// RUN: %dxc -E main -enable-unions -HV 202x -T vs_6_2 %s | FileCheck %s

union s0 {
  int a;
  double b;
  float c;
};

// CHECK: error: double(type for OUT) cannot be used as shader inputs or outputs.
s0 main() : OUT {
  s0 s;
  return s;
}
