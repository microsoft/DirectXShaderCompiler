// RUN: %dxc -Tcs_6_6 -Emain /Od /Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

// Verify a member after a 128-bit struct with 32 bits of tail padding.

RWByteAddressBuffer RawUAV : register(u0);

struct TailPadded
{
    double Wide;
    float Narrow;
};

struct Holder
{
    TailPadded Padded;
    float Trailing;
};

// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 0, 64)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 64, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 128, 32)

// CHECK: store double
// CHECK: store float
// CHECK: store float

[numthreads(1, 1, 1)]
void main()
{
    Holder holder;
    holder.Padded.Wide = (double)RawUAV.Load(2 * 4);
    holder.Padded.Narrow = (float)RawUAV.Load(3 * 4);
    holder.Trailing = (float)RawUAV.Load(7 * 4);

    RawUAV.Store(0, (float)holder.Padded.Wide + holder.Padded.Narrow + holder.Trailing);
}
