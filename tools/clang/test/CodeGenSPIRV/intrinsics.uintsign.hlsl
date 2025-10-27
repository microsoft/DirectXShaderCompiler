// RUN: %dxc -T vs_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK:      [[glsl:%[0-9]+]] = OpExtInstImport "GLSL.std.450"

void main() {
  int result;
  int3 result3;

// CHECK: [[a:%[0-9]+]] = OpLoad %uint %a
// CHECK: [[abs_a:%[0-9]+]] = OpExtInst %int [[glsl]] SAbs [[a]]
// CHECK-NEXT: [[sign_a:%[0-9]+]] = OpExtInst %int [[glsl]] SSign [[abs_a]]
// CHECK-NEXT: OpStore %result [[sign_a]]
  uint a;
  result = sign(a);

// CHECK-NEXT: [[b:%[0-9]+]] = OpLoad %uint %b
// CHECK: [[abs_b:%[0-9]+]] = OpExtInst %int [[glsl]] SAbs [[b]]
// CHECK-NEXT: [[sign_b:%[0-9]+]] = OpExtInst %int [[glsl]] SSign [[abs_b]]
// CHECK-NEXT: OpStore %result [[sign_b]]
  uint1 b;
  result = sign(b);

// CHECK-NEXT: [[c:%[0-9]+]] = OpLoad %v3uint %c
// CHECK: [[abs_c:%[0-9]+]] = OpExtInst %v3int [[glsl]] SAbs [[c]]
// CHECK-NEXT: [[sign_c:%[0-9]+]] = OpExtInst %v3int [[glsl]] SSign [[abs_c]]
// CHECK-NEXT: OpStore %result3 [[sign_c]]
  uint3 c;
  result3 = sign(c);

// CHECK:      [[d:%[0-9]+]] = OpLoad %uint %d
// CHECK: [[abs_d:%[0-9]+]] = OpExtInst %int [[glsl]] SAbs [[d]]
// CHECK-NEXT: [[sign_d:%[0-9]+]] = OpExtInst %int [[glsl]] SSign [[abs_d]]
// CHECK-NEXT: OpStore %result [[sign_d]]
  uint1x1 d;
  result = sign(d);

// CHECK-NEXT: [[e:%[0-9]+]] = OpLoad %v2uint %e
// CHECK-NEXT: [[abs_e:%[0-9]+]] = OpExtInst %v2int [[glsl]] SAbs [[e]]
// CHECK-NEXT: [[sign_e:%[0-9]+]] = OpExtInst %v2int [[glsl]] SSign [[abs_e]]
// CHECK-NEXT: OpStore %result2 [[sign_e]]
  uint1x2 e;
  int2 result2 = sign(e);

// CHECK-NEXT: [[f:%[0-9]+]] = OpLoad %v4uint %f
// CHECK-NEXT: [[abs_f:%[0-9]+]] = OpExtInst %v4int [[glsl]] SAbs [[f]]
// CHECK-NEXT: [[sign_f:%[0-9]+]] = OpExtInst %v4int [[glsl]] SSign [[abs_f]]
// CHECK-NEXT: OpStore %result4 [[sign_f]]
  uint4x1 f;
  int4 result4 = sign(f);

// TODO: Integer matrices are not supported yet. See intrinsics.intsign.hlsl
}