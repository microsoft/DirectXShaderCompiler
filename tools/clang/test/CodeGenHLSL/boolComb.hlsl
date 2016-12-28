// RUN: %dxc -E main -T ps_5_0 %s | FileCheck %s

bool cb;

float4 main(float4 a : A) : SV_TARGET {
  bool b = cb;
// cmp with 0 for any and all.
// CHECK: fcmp fast une float
// CHECK: , 0.000000e+00
// CHECK: fcmp fast une float
// CHECK: , 0.000000e+00
// CHECK: fcmp fast une float
// CHECK: , 0.000000e+00
// CHECK: fcmp fast une float
// CHECK: , 0.000000e+00
// or i1 for any.
// CHECK: or i1
// CHECK: or i1
// CHECK: or i1
// convert any to int.
// CHECK: zext i1
// |=
// CHECK: or i32
  b |= any(a);

// and i1 for all.
// CHECK: and i1
// CHECK: and i1
// CHECK: and i1
// convert all to int.
// CHECK: zext i1
// CHECK: and i32
  b &= all(a);
// cmp for a>2.
// CHECK: fcmp fast ogt float
// CHECK: 2.000000e+00
// CHECK: fcmp fast ogt float
// CHECK: 2.000000e+00
// CHECK: fcmp fast ogt float
// CHECK: 2.000000e+00
// CHECK: fcmp fast ogt float
// CHECK: 2.000000e+00
// or i1 for any
// CHECK: or i1
// CHECK: or i1
// CHECK: or i1
// convert any to int.
// CHECK: zext i1
//  %xor = xor i32 b, any
//  %tobool15 = icmp ne i32 %xor, 0
// will be translated into
//  icmp ne i32 b, any
// CHECK: icmp ne i32
  b ^= any(a>2);
  return b;
}
