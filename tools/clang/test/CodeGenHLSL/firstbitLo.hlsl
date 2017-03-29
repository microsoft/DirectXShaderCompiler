// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: FirstbitLo

// CHECK: FirstbitLo

// CHECK: FirstbitLo
// CHECK: FirstbitLo

// CHECK: dx.op.unaryBits.i64
// CHECK: FirstbitLo

uint a;
int2 b;


RWByteAddressBuffer outputUAV;

float4 main() : SV_TARGET
{
    outputUAV.Store(0, firstbitlow(a));
    outputUAV.Store(1, firstbitlow(b).y);
    outputUAV.Store(2, firstbitlow(32));
    outputUAV.Store(3, firstbitlow(-512));
    uint64_t c = b.x + 1;
    outputUAV.Store(4, firstbitlow(c));

    return 1.0;
}
