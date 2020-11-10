// RUN: %dxc -E main -T cs_6_0 -Zi %s | FileCheck %s

// CHECK-DAG: call void @llvm.dbg.value(metadata i16 1, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"foo" !DIExpression(DW_OP_bit_piece, 0, 16)
// CHECK-DAG: call void @llvm.dbg.value(metadata i16 2, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"foo" !DIExpression(DW_OP_bit_piece, 32, 16)

struct Foo
{
    min16int m_A;
    min16int m_B;
};

[numthreads(1, 1, 1)]
[RootSignature("")]
void main()
{
    Foo foo = { 1, 2 };
    min16int value1 = foo.m_B;
    min16int value2 = foo.m_A;
}
