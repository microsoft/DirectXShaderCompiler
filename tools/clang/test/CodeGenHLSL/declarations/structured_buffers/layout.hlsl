// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s

// Tests the printed layout of structured buffers.

// CHECK: int2 a; ; Offset: 0
// CHECK: int b[2]; ; Offset: 8
// CHECK: int2 c; ; Offset: 16
// CHECK: int2 d; ; Offset: 24
// CHECK: int e; ; Offset: 32
// CHECK: Size: 36

// CHECK: int2 a; ; Offset: 0
// CHECK: int b[2]; ; Offset: 8
// CHECK: int2 c; ; Offset: 16
// CHECK: int2 d; ; Offset: 24
// CHECK: int e; ; Offset: 32
// CHECK: Size: 36

struct Struct
{
    int2 a;
    struct
    {
        int b[2];
        int2 c;
        int2 d;
    } s;
    int e;
};

StructuredBuffer<Struct> sb;
RWStructuredBuffer<Struct> rwsb;

int main() : OUT
{
    return sb[0].e + rwsb[0].e;
}