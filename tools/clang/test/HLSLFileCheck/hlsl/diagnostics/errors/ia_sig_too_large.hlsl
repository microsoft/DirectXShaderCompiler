// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Expect error when the Input Assembler (vertex input) signature is too large
// CHECK: error: Failed to allocate all input signature elements in available space.

struct VSIN {
    float4 pos : Position;
    float array1[30] : ARRAY1;
    float array2[2] : ARRAY2;
};

float4 main(VSIN In) : SV_Position {
    return In.pos;
}
