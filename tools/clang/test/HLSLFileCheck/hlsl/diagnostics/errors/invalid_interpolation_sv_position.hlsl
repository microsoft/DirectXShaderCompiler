// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: error: invalid interpolation mode for SV_Position

struct PSIN {
    nointerpolation float4 pos : SV_Position;
};

float main(PSIN In) : SV_Target{
    return In.pos.x;
}
