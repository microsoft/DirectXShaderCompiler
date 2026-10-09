// RUN: %dxc -Tcs_6_6 -enable-16bit-types -Emain /Od /Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

// Verify a 16-bit member after a tail-padded 64-bit struct.

RWByteAddressBuffer RawUAV : register(u0);

struct HalfTail
{
    float Wide;
    float16_t Small;
};

struct Holder
{
    HalfTail Padded;
    float16_t Trailing;
};

// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 0, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 32, 16)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 64, 16)

// CHECK: store float
// CHECK: store half
// CHECK: store half

[numthreads(1, 1, 1)]
void main()
{
    Holder holder;
    holder.Padded.Wide = (float)RawUAV.Load(2 * 4);
    holder.Padded.Small = (float16_t)RawUAV.Load(3 * 4);
    holder.Trailing = (float16_t)RawUAV.Load(4 * 4);

    RawUAV.Store(0, holder.Padded.Wide + (float)holder.Padded.Small +
                        (float)holder.Trailing);
}
