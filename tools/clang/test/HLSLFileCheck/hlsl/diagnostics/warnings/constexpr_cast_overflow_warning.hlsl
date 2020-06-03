// RUN: %dxc -E real_lit_to_flt_warning -T vs_6_0 %s | FileCheck %s
// CHECK: warning:

// Verify that when a constant is cast to a different type leading to overflow
// a warning is generated notifying the same. It also verifies that if no overflow
// happens, then no warning is reported

float real_lit_to_flt_warning() : OUT {  
  return 3.4e50;
}

float real_lit_to_flt_nowarning() : OUT {
  return 3.4e10;
}

min16float real_lit_to_half_warning() : OUT {
  return 65520.0;
}

min16float real_lit_to_half_nowarning() : OUT {
  return 65500.0;
}

min16float int_lit_to_half_warning() : OUT {
  return 65520;
}

min16float int_lit_to_half_nowarning() : OUT {
  return 65500;
}