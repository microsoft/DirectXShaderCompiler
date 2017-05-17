// RUN: %dxc -E main -T ps_6_1 -HV 2017 %s | FileCheck %s

// CHECK: fadd

enum Vertex {
    FIRST,
    SECOND,
    THIRD
};

float4 main(float4 col : COLOR) : SV_Target {
    return !Vertex::FIRST + col;
}
