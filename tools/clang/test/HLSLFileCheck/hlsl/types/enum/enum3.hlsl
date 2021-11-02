// RUN: %dxc -E main -T ps_6_1 -HV 2017 %s | FileCheck %s
// REQUIRES: dxilver-1.2

// CHECK: dx.op.attributeAtVertex

enum Vertex {
    FIRST,
    SECOND,
    THIRD
};

int4 main(nointerpolation float4 col : COLOR) : SV_Target {
    return GetAttributeAtVertex(col, Vertex::THIRD);
}