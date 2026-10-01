// RUN: %dxc -Tcs_6_6 -Emain /Od /Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

// Verify an array of 128-bit padded structs followed by another member.

RWByteAddressBuffer RawUAV : register(u0);

struct TailPadded
{
    double Wide;
    float Narrow;
};

struct Holder
{
    TailPadded Elements[2];
    float Trailing;
};

// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 0, 64)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 64, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 128, 64)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 192, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 256, 32)

// CHECK: store double
// CHECK: store float
// CHECK: store double
// CHECK: store float
// CHECK: store float

[numthreads(1, 1, 1)]
void main()
{
    Holder holder;
    holder.Elements[0].Wide = (double)RawUAV.Load(2 * 4);
    holder.Elements[0].Narrow = (float)RawUAV.Load(3 * 4);
    holder.Elements[1].Wide = (double)RawUAV.Load(4 * 4);
    holder.Elements[1].Narrow = (float)RawUAV.Load(5 * 4);
    holder.Trailing = (float)RawUAV.Load(7 * 4);

    RawUAV.Store(0, (float)holder.Elements[0].Wide + holder.Elements[0].Narrow +
                        (float)holder.Elements[1].Wide + holder.Elements[1].Narrow +
                        holder.Trailing);
}
