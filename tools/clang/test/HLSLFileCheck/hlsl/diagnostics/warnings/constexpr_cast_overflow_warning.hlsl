// RUN: %dxc -E real_lit_to_flt_warning -T vs_6_0 %s | FileCheck %s
// RUN: %dxc -E real_lit_to_half_warning -T vs_6_0 %s | FileCheck %s
// RUN: %dxc -E int_lit_to_half_warning -T vs_6_0 %s | FileCheck %s
// CHECK: warning: overflow in the expression

// Verify that when a constant is cast to a different type leading to overflow
// a warning is generated notifying the same.

float real_lit_to_flt_warning() {  
  return 3.4e50;
}

min16float real_lit_to_half_warning() {
  return 65520.0;
}

min16float int_lit_to_half_warning() {
  return 65520;
}