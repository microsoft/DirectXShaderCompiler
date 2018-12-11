// Run: %dxc -T ps_6_2 -E main -enable-16bit-types

// CHECK: [[v2c31:%\d+]] = OpConstantComposite %v2uint %uint_31 %uint_31
// CHECK: [[v3c63:%\d+]] = OpConstantComposite %v3long %long_63 %long_63 %long_63
// CHECK: [[v4c15:%\d+]] = OpConstantComposite %v4ushort %ushort_15 %ushort_15 %ushort_15 %ushort_15
void main() {
    int       a, b;
    uint2     d, e;

    int64_t3  g, h;
    uint64_t  j, k;

    int16_t   m, n;
    uint16_t4 p, q;

// CHECK:        [[b:%\d+]] = OpLoad %int %b
// CHECK:      [[rhs:%\d+]] = OpBitwiseAnd %int [[b]] %int_31
// CHECK-NEXT:                OpShiftLeftLogical %int {{%\d+}} [[rhs]]
    a <<= b;

// CHECK:        [[e:%\d+]] = OpLoad %v2uint %e
// CHECK:      [[rhs:%\d+]] = OpBitwiseAnd %v2uint [[e]] [[v2c31]]
// CHECK-NEXT:                OpShiftLeftLogical %v2uint {{%\d+}} [[rhs]]
    d <<= e;

// CHECK:        [[h:%\d+]] = OpLoad %v3long %h
// CHECK:      [[rhs:%\d+]] = OpBitwiseAnd %v3long [[h]] [[v3c63]]
// CHECK-NEXT:                OpShiftLeftLogical %v3long {{%\d+}} [[rhs]]
    g <<= h;

// CHECK:        [[k:%\d+]] = OpLoad %ulong %k
// CHECK:      [[rhs:%\d+]] = OpBitwiseAnd %ulong [[k]] %ulong_63
// CHECK-NEXT:                OpShiftLeftLogical %ulong {{%\d+}} [[rhs]]
    j <<= k;

// CHECK:        [[n:%\d+]] = OpLoad %short %n
// CHECK:      [[rhs:%\d+]] = OpBitwiseAnd %short [[n]] %short_15
// CHECK-NEXT:                OpShiftLeftLogical %short {{%\d+}} [[rhs]]
    m <<= n;

// CHECK:        [[q:%\d+]] = OpLoad %v4ushort %q
// CHECK:      [[rhs:%\d+]] = OpBitwiseAnd %v4ushort [[q]] [[v4c15]]
// CHECK-NEXT:                OpShiftLeftLogical %v4ushort {{%\d+}} [[rhs]]
    p <<= q;
}
