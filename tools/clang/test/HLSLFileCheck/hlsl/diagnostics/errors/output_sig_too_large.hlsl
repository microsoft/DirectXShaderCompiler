// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Expect error when the output signature is too large
// CHECK: error: Failed to allocate all output signature elements in available space.

struct VSOUT {
    float4 pos : SV_Position;
    float4 array1[30] : BIGARRAY;
    float4 array2[2] : SMALLARRAY;
};

VSOUT main() {
    VSOUT Out = (VSOUT)0;
    return Out;
}
