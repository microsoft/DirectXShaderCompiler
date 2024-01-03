// RUN: not %dxc -T ps_6_0 -fcgl -spirv %s 2>&1 | FileCheck %s

// CHECK: error: integer literal selectors in switch statements not yet implemented

float main() : SV_TARGET {
  switch (0) {
  case 0:
    return 1;
  }
  return 0;
}
