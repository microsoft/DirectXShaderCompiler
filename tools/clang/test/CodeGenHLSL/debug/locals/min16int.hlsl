// RUN: %dxc -E main -T vs_6_0 -Zi -Od %s | FileCheck %s

// CHECK-DAG: call void @llvm.dbg.value(metadata i16 2, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"value1" !DIExpression()
// CHECK-DAG: call void @llvm.dbg.value(metadata i16 1, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"value2" !DIExpression()

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
