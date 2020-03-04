// RUN: %dxc -E main -T ps_6_0 %s -Zi -Od | FileCheck %s

[RootSignature("")]
float main(float a : A) : SV_Target {
  float2 xy = float2(0,0);
  xy.x = sin(a);
  // CHECK: call float @dx.op.unary.f32(i32 13,
  // CHECK-SAME: line:6

  // CHECK: call void @llvm.dbg.value(
  // CHECK-SAME: var:"xy"
  // CHECK-SAME: !DIExpression(DW_OP_bit_piece, 0, 32)

  xy.y = cos(xy.x);
  // CHECK: call float @dx.op.unary.f32(i32 12,
  // CHECK-SAME: line:14

  // CHECK: call void @llvm.dbg.value(
  // CHECK-SAME: var:"xy"
  // CHECK-SAME: !DIExpression(DW_OP_bit_piece, 32, 32)

  float z = abs(xy.y);
  // CHECK: call float @dx.op.unary.f32(i32 6,
  // CHECK-SAME: line:22

  // CHECK: call void @llvm.dbg.value(
  // CHECK-SAME: var:"z"
  // CHECK-SAME: !DIExpression()

  float w = tan(z);
  // CHECK: call float @dx.op.unary.f32(i32 14,
  // CHECK-SAME: line:30

  // CHECK: call void @llvm.dbg.value(
  // CHECK-SAME: var:"w"
  // CHECK-SAME: !DIExpression()

  return w;
}

