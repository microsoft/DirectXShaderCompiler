// Run: %dxc -T ps_6_0 -E main

// The purpose of this test is to make sure non-precise computations are *not*
// decorated with NoContraction.
//
// To this end, we will perform the same computation twice, once when it
// affects a precise variable, and once when it doesn't.

void foo(float p) { p = p + 1; }

// CHECK:      OpName %bb_entry_0 "bb.entry"
// CHECK-NEXT: OpDecorate [[first_b_plus_c:%\d+]] NoContraction
// CHECK-NOT:  OpDecorate [[first_a_mul_b]] NoContraction
// CHECK-NOT:  OpDecorate [[ax_mul_bx]] NoContraction
// CHECK-NEXT: OpDecorate [[second_a_mul_b:%\d+]] NoContraction
// CHECK-NOT:  OpDecorate [[second_a_plus_b]] NoContraction
// CHECK-NEXT: OpDecorate [[first_d_plus_e:%\d+]] NoContraction
// CHECK-NEXT: OpDecorate [[c_mul_d:%\d+]] NoContraction
// CHECK-NOT:  OpDecorate [[second_d_plus_e]] NoContraction
// CHECK-NEXT: OpDecorate [[r_plus_s:%\d+]] NoContraction

void main() {
  float4 a, b, c, d, e;
  precise float4 v; 
  float3 r, s, u;

// This can change "a" which can then change "r" which can then change "v". Precise.
//
// CHECK:                           OpLoad %v4float %b
// CHECK-NEXT:                      OpLoad %v4float %c
// CHECK-NEXT: [[first_b_plus_c]] = OpFAdd %v4float
// CHECK-NEXT:                      OpStore %a %29
  a = b + c;
  
// Even though this looks like the statement on line 52:
// This changes "u", which does not affect "v" in any way. Not Precise.
//
// CHECK:      [[first_a_mul_b:%\d+]] = OpFMul %v3float
// CHECK-NEXT:                          OpStore %u
  u = float3((float3)a * (float3)b);
  
// Does not affect the value of "v". Not Precise.
//
// CHECK:      [[ax_mul_bx:%\d+]] = OpFMul %float
// CHECK-NEXT:                      OpStore %param_var_p
  foo(a.x * b.x);

// This changes "r" which will later change "v". Precise.
//
// CHECK:      [[second_a_mul_b]] = OpFMul %v3float
// CHECK-NEXT:                      OpStore %r %58
  r = float3((float3)a * (float3)b);

// Even though this looks identical to "a = b + c" above:
// This can change the value of "a", BUT, this change will not affect "v". Not Precise.
//
// CHECK:      [[second_a_plus_b:%\d+]] = OpFAdd %v4float
// CHECK-NEXT:                            OpStore %a %61
  a = b + c;

// This can change "c" which can then change "s" which can then change "v". Precise.
//
// CHECK:                           OpLoad %v4float %d
// CHECK-NEXT:                      OpLoad %v4float %e
// CHECK-NEXT: [[first_d_plus_e]] = OpFAdd %v4float
// CHECK-NEXT:                      OpStore %c
  c = d + e;

// This changes "s" which will later change "v". Precise.
//
// CHECK:      [[c_mul_d]] = OpFMul %v3float
// CHECK-NEXT:               OpStore %s %75
  s = float3((float3)c * (float3)d);

// Even though this looks identical to "c = d + e" above:
// This can change the value of "c", BUT, this change will not affect "v". Not Precise.
//
// CHECK:                                 OpLoad %v4float %d
// CHECK-NEXT:                            OpLoad %v4float %e
// CHECK-NEXT: [[second_d_plus_e:%\d+]] = OpFAdd %v4float
// CHECK-NEXT:                            OpStore %c
  c = d + e;

// Precise because "v" is precise.
// CHECK:                OpLoad %v3float %r
// CHECK:                OpLoad %v3float %s
// CHECK: [[r_plus_s]] = OpFAdd %v3float
// CHECK:                OpStore %v
  v.xyz = r + s;
}

