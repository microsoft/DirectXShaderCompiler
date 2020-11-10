// RUN: %dxc -E main -T vs_6_0 -Zi %s | FileCheck %s

// CHECK-DAG: call void @llvm.dbg.value(metadata half 0xH4000, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"value1" !DIExpression()
// CHECK-DAG: call void @llvm.dbg.value(metadata half 0xH3000, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"value2" !DIExpression()

struct Foo
{
    min16float m_A;
    min16float m_B;
};

[numthreads(1, 1, 1)]
[RootSignature("")]
void main()
{
    Foo foo = { 1, 2 };
    min16float value1 = foo.m_B;
    min16float value2 = foo.m_A;
}
