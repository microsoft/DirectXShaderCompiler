// RUN: %dxc -Tcs_6_6 -Emain /Od /Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

// Verify declared offsets across nested tail-padded aggregates.

RWByteAddressBuffer RawUAV : register(u0);

struct Leaf
{
    double Wide;
    float Narrow;
};

struct Middle
{
    Leaf Nested;
    float AfterNested;
};

struct Root
{
    Middle Inner;
    float Trailing;
};

// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 0, 64)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 64, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 128, 32)
// CHECK: dbg.declare{{.*}}!DIExpression(DW_OP_bit_piece, 192, 32)

// CHECK: store double
// CHECK: store float
// CHECK: store float
// CHECK: store float

[numthreads(1, 1, 1)]
void main()
{
    Root root;
    root.Inner.Nested.Wide = (double)RawUAV.Load(2 * 4);
    root.Inner.Nested.Narrow = (float)RawUAV.Load(3 * 4);
    root.Inner.AfterNested = (float)RawUAV.Load(4 * 4);
    root.Trailing = (float)RawUAV.Load(5 * 4);

    RawUAV.Store(0, (float)root.Inner.Nested.Wide + root.Inner.Nested.Narrow +
                        root.Inner.AfterNested + root.Trailing);
}
