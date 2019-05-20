// Run: %dxc -T ps_6_2 -E main -enable-16bit-types

// CHECK: [[v2c31:%\d+]] = OpConstantComposite %v2uint %uint_31 %uint_31
// CHECK: [[v3c63:%\d+]] = OpConstantComposite %v3long %long_63 %long_63 %long_63
// CHECK: [[v4c15:%\d+]] = OpConstantComposite %v4ushort %ushort_15 %ushort_15 %ushort_15 %ushort_15
void main() {
    int       a, b, c;
    uint2     d, e, f;

    int64_t3  g, h, i;
    uint64_t  j, k, l;

    int16_t   m, n, o;
    uint16_t4 p, q, r;

// CHECK:        [[b:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[rhs:%\d+]] = OpBitwiseAnd %int [[b]] %int_31
// CHECK-NEXT:                OpShiftRightArithmetic %int {{%\d+}} [[rhs]]
    c = a >> b;

// CHECK:        [[e:%\d+]] = OpLoad %v2uint %e
// CHECK-NEXT: [[rhs:%\d+]] = OpBitwiseAnd %v2uint [[e]] [[v2c31]]
// CHECK-NEXT:                OpShiftRightLogical %v2uint {{%\d+}} [[rhs]]
    f = d >> e;

// CHECK:        [[h:%\d+]] = OpLoad %v3long %h
// CHECK-NEXT: [[rhs:%\d+]] = OpBitwiseAnd %v3long [[h]] [[v3c63]]
// CHECK-NEXT:                OpShiftRightArithmetic %v3long {{%\d+}} [[rhs]]
    i = g >> h;

// CHECK:        [[k:%\d+]] = OpLoad %ulong %k
// CHECK-NEXT: [[rhs:%\d+]] = OpBitwiseAnd %ulong [[k]] %ulong_63
// CHECK-NEXT:                OpShiftRightLogical %ulong {{%\d+}} [[rhs]]
    l = j >> k;

// CHECK:        [[n:%\d+]] = OpLoad %short %n
// CHECK-NEXT: [[rhs:%\d+]] = OpBitwiseAnd %short [[n]] %short_15
// CHECK-NEXT:                OpShiftRightArithmetic %short {{%\d+}} [[rhs]]
    o = m >> n;

// CHECK:        [[q:%\d+]] = OpLoad %v4ushort %q
// CHECK-NEXT: [[rhs:%\d+]] = OpBitwiseAnd %v4ushort [[q]] [[v4c15]]
// CHECK-NEXT:                OpShiftRightLogical %v4ushort {{%\d+}} [[rhs]]
    r = p >> q;
}
