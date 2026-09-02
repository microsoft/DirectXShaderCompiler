// RUN: %dxc -Tcs_6_6 -Emain /Od /Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

// Verify matrix offsets after a 128-bit tail-padded struct.

RWByteAddressBuffer RawUAV : register(u0);

struct TailPadded
{
    double Wide;
    float Narrow;
};

struct Holder
{
    TailPadded Padded;
    float2x2 Mat;
};

// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 0, 64)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 64, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 128, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 160, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 192, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 224, 32)

[numthreads(1, 1, 1)]
void main()
{
    Holder holder;
    holder.Padded.Wide = (double)RawUAV.Load(2 * 4);
    holder.Padded.Narrow = (float)RawUAV.Load(3 * 4);
    holder.Mat = float2x2(RawUAV.Load(4 * 4), RawUAV.Load(5 * 4),
                          RawUAV.Load(6 * 4), RawUAV.Load(7 * 4));

    RawUAV.Store(0, (float)holder.Padded.Wide + holder.Padded.Narrow +
                        holder.Mat._11 + holder.Mat._22);
}
