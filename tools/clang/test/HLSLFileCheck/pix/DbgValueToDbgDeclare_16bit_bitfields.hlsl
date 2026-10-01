// RUN: %dxc -Tcs_6_6 -enable-16bit-types -Emain /Od /Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

// Verify that 16-bit and 32-bit bitfields use their declared storage units.

RWByteAddressBuffer RawUAV : register(u0);

struct HalfBitfield
{
    uint16_t Small : 5;
    uint32_t Wide : 20;
};

// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 0, 5)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 32, 20)

// CHECK: store i16 %{{[^,]+}}, i16*
// CHECK: store i32 %{{[^,]+}}, i32*

[numthreads(1, 1, 1)]
void main()
{
    HalfBitfield bitfield;
    bitfield.Small = (uint16_t)RawUAV.Load(2 * 4);
    bitfield.Wide = RawUAV.Load(3 * 4);

    RawUAV.Store(0, bitfield.Small + bitfield.Wide);
}
