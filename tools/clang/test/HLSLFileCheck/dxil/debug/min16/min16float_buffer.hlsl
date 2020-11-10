// RUN: %dxc -E main -T cs_6_0 -Zi %s | FileCheck %s

// CHECK: Foo = type { half, half }

struct Foo
{
    min16float m_A;
    min16float m_B;
};

StructuredBuffer<Foo> buf;

[numthreads(1, 1, 1)]
[RootSignature("")]
void main()
{
    Foo foo = buf[0];
    min16float value1 = foo.m_B;
    min16float value2 = foo.m_A;
}

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

