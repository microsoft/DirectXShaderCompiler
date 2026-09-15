// RUN: %dxc -T cs_6_0 -E main -HV 202x -fcgl %s -spirv | FileCheck %s

// Test that the 'auto' keyword can be used as a function return type and
// that the deduced type is used correctly when targeting SPIR-V.

// CHECK-DAG: [[INT:%[a-zA-Z0-9_]+]] = OpTypeInt 32 1
// CHECK-DAG: [[FLOAT:%[a-zA-Z0-9_]+]] = OpTypeFloat 32
// CHECK-DAG: [[V4FLOAT:%[a-zA-Z0-9_]+]] = OpTypeVector [[FLOAT]] 4

// Function 'SquareInt' must return int.
// CHECK-DAG: %SquareInt = OpFunction [[INT]] None
// CHECK-DAG: %SquareFloat = OpFunction [[FLOAT]] None
// CHECK-DAG: %Scale = OpFunction [[V4FLOAT]] None
// CHECK-DAG: %WriteOutput = OpFunction %void None

RWBuffer<float> output : register(u0);

auto SquareInt(int x) {
    return x * x;
}

auto SquareFloat(float x) {
    return x * x;
}

auto Scale(float4 v, float s) {
    return v * s;
}

auto WriteOutput(uint i, float v) {
    output[i] = v;
}

[numthreads(1,1,1)]
void main() {
    float4 v = float4(1, 2, 3, 4);
    float4 s = Scale(v, 0.5f);
    WriteOutput(0, (float)SquareInt(3) + SquareFloat(2.5f) + s.x);
}
