// RUN: %dxc -Tcs_6_6 -Emain /Od /Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

// Verify declared offsets for front-padded and tail-padded structs.

RWByteAddressBuffer RawUAV : register(u0);

struct FrontPadded
{
    float Narrow;
    double Wide;
};

struct TailPadded
{
    double Wide;
    float Narrow;
};

struct Holder
{
    FrontPadded Front;
    TailPadded Tail;
    float Trailing;
};

// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 0, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 64, 64)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 128, 64)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 192, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 256, 32)

// CHECK: store float
// CHECK: store double
// CHECK: store double
// CHECK: store float
// CHECK: store float

[numthreads(1, 1, 1)]
void main()
{
    Holder holder;
    holder.Front.Narrow = (float)RawUAV.Load(2 * 4);
    holder.Front.Wide = (double)RawUAV.Load(3 * 4);
    holder.Tail.Wide = (double)RawUAV.Load(4 * 4);
    holder.Tail.Narrow = (float)RawUAV.Load(5 * 4);
    holder.Trailing = (float)RawUAV.Load(6 * 4);

    RawUAV.Store(0, holder.Front.Narrow + (float)holder.Front.Wide +
                        (float)holder.Tail.Wide + holder.Tail.Narrow + holder.Trailing);
}
