// RUN: %dxc -T vs_6_0 -E main

// CHECK: error: elements for SV_ClipDistance variable 'foo' must be scalar, vector, or array with float type

struct VS_OUTPUT {
    int3 foo[2] : SV_ClipDistance0;
    float4 bar[2] : SV_ClipDistance1;
};

VS_OUTPUT main() {
    return (VS_OUTPUT) 0;
}
