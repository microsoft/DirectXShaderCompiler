// RUN: %dxc -Tcs_6_6 -Emain /Od /Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

// Verify that mixed-width bitfields use their declared storage units.

RWByteAddressBuffer RawUAV : register(u0);

struct MixedWidthBitfield
{
    uint32_t Leading : 5;
    uint64_t Middle : 59;
    uint32_t Trailing : 5;
};

// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 0, 5)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 64, 59)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 128, 5)

// CHECK: store i32 %{{[^,]+}}, i32*
// CHECK: store i64 %{{[^,]+}}, i64*
// CHECK: store i32 %{{[^,]+}}, i32*

[numthreads(1, 1, 1)]
void main()
{
    MixedWidthBitfield bitfield;
    bitfield.Leading = RawUAV.Load(9 * 4);
    bitfield.Middle = RawUAV.Load(21 * 4);
    bitfield.Trailing = RawUAV.Load(13 * 4);

    RawUAV.Store(0, (uint)(bitfield.Leading + bitfield.Middle + bitfield.Trailing));
}
