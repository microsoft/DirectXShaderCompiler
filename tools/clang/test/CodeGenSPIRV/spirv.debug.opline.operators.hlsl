// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.operators.hlsl

static int a, b, c;

// Note that preprocessor prepends a "#line 1 ..." line to the whole file and
// the compliation sees line numbers incremented by 1.

void main() {
// CHECK:      OpLine [[file]] 15 13
// CHECK-NEXT: OpIAdd %int
  int d = a + b;

// CHECK:      OpLine [[file]] 19 9
// CHECK-NEXT: OpIMul %int
  c = a * b;

// CHECK:      OpLine [[file]] 23 23
// CHECK-NEXT: OpISub %int
  /* comment */ c = a - b;

// CHECK:      OpLine [[file]] 27 9
// CHECK-NEXT: OpSDiv %int
  c = a / b;

// CHECK:      OpLine [[file]] 31 10
// CHECK-NEXT: OpSLessThan %bool
  c = (a < b);

// CHECK:      OpLine [[file]] 35 9
// CHECK-NEXT: OpSGreaterThan %bool
  c = a > b;

// CHECK:      OpLine [[file]] 39 11
// CHECK-NEXT: OpLogicalAnd %bool
  c = (a) && b;

// CHECK:      OpLine [[file]] 43 5
// CHECK-NEXT: OpLogicalOr %bool
  a || b;

// CHECK: OpLine [[file]] 47 5
// CHECK: OpShiftLeftLogical %int
  a << b;

// CHECK: OpLine [[file]] 51 9
// CHECK: OpShiftRightArithmetic %int
  c = a >> b;

// CHECK:      OpLine [[file]] 55 9
// CHECK-NEXT: OpBitwiseAnd %int
  c = a & b;

// CHECK:      OpLine [[file]] 59 7
// CHECK-NEXT: OpNot %int
  c = ~b;

// CHECK:      OpLine [[file]] 63 9
// CHECK-NEXT: OpBitwiseXor %int
  c = a ^ b;

// CHECK:      OpLine [[file]] 71 5
// CHECK-NEXT: OpIAdd %int
// CHECK:      OpLine [[file]] 71 11
// CHECK-NEXT: OpNot %int
// CHECK-NEXT: OpLine [[file]] 71 9
// CHECK-NEXT: OpBitwiseXor %int
  c + a ^ ~b;

// CHECK:      OpLine [[file]] 75 3
// CHECK-NEXT: OpIAdd %int
  ++a;

// CHECK:      OpLine [[file]] 79 4
// CHECK-NEXT: OpIAdd %int
  a++;

// CHECK:      OpLine [[file]] 83 4
// CHECK-NEXT: OpISub %int
  a--;

// CHECK:      OpLine [[file]] 87 3
// CHECK-NEXT: OpISub %int
  --a;

// CHECK: OpLine [[file]] 91 5
// CHECK: OpShiftLeftLogical %int
  a <<= 10;

// CHECK:      OpLine [[file]] 95 5
// CHECK-NEXT: OpISub %int
  a -= 10;

// CHECK:      OpLine [[file]] 99 5
// CHECK-NEXT: OpIMul %int
  a *= 10;

// CHECK:      OpLine [[file]] 107 15
// CHECK-NEXT: OpIAdd %int
// CHECK-NEXT: OpLine [[file]] 107 10
// CHECK-NEXT: OpIAdd %int
// CHECK:      OpLine [[file]] 107 5
// CHECK-NEXT: OpSDiv %int
  a /= d + (b + c);

// CHECK:      OpLine [[file]] 113 10
// CHECK-NEXT: OpSLessThan %bool
// CHECK-NEXT: OpLine [[file]] 113 15
// CHECK-NEXT: OpLogicalAnd %bool
  b = (a < c) && true;

// CHECK:      OpLine [[file]] 117 5
// CHECK-NEXT: OpIAdd %int
  a += c;

// CHECK:      OpLine [[file]] 125 15
// CHECK-NEXT: OpIMul %int %int_100
// CHECK:      OpLine [[file]] 125 25
// CHECK-NEXT: OpISub %int %int_20
// CHECK-NEXT: OpLine [[file]] 125 19
// CHECK-NEXT: OpSDiv %int
  d = a + 100 * b / (20 - c);
// CHECK-NEXT: OpLine [[file]] 125 9
// CHECK-NEXT: OpIAdd %int

  float2x2 m2x2f;
  int2x2 m2x2i;

// CHECK:      OpLine [[file]] 136 13
// CHECK-NEXT: OpMatrixTimesScalar %mat2v2float
// CHECK:      OpLine [[file]] 136 21
// CHECK-NEXT: OpFAdd %v2float
  m2x2f = 2 * m2x2f + m2x2i;

// CHECK:      OpLine [[file]] 144 17
// CHECK-NEXT: OpFMul %v2float
// CHECK:      OpLine [[file]] 144 17
// CHECK-NEXT: OpFMul %v2float
// CHECK-NEXT: OpLine [[file]] 144 11
// CHECK-NEXT: OpCompositeConstruct %mat2v2float
  m2x2f = m2x2f * m2x2i;

  float4 v4f;
  int4 v4i;

// CHECK:      OpLine [[file]] 151 13
// CHECK-NEXT: OpFDiv %v4float
  v4i = v4f / v4i;

// CHECK:      OpLine [[file]] 159 17
// CHECK-NEXT: OpFMul %v2float
// CHECK:      OpLine [[file]] 159 17
// CHECK-NEXT: OpFMul %v2float
// CHECK-NEXT: OpLine [[file]] 159 11
// CHECK-NEXT: OpCompositeConstruct %mat2v2float
  m2x2f = m2x2f * v4f;

// CHECK:      OpLine [[file]] 163 17
// CHECK-NEXT: OpMatrixTimesScalar %mat2v2float
  m2x2f = m2x2f * v4f.x;

// CHECK:      OpLine [[file]] 169 8
// CHECK-NEXT: OpIMul %v4int
// CHECK:      OpLine [[file]] 169 13
// CHECK-NEXT: OpFOrdLessThan %v4bool
  (v4i * a) < m2x2f;

// CHECK:      OpLine [[file]] 175 8
// CHECK-NEXT: OpSDiv %v4int
// CHECK:      OpLine [[file]] 175 13
// CHECK-NEXT: OpLogicalAnd %v4bool
  (v4i / a) && v4f;
}
