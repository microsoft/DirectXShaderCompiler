// RUN: %dxc -E main -T cs_6_0 -Zi %s | FileCheck %s

// CHECK: Foo = type { i16, i16 }

struct Foo
{
    min16int m_A;
    min16int m_B;
};

StructuredBuffer<Foo> buf;

[numthreads(1, 1, 1)]
[RootSignature("")]
void main()
{
    Foo foo = buf[0];
    min16int value1 = foo.m_B;
    min16int value2 = foo.m_A;
}

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

