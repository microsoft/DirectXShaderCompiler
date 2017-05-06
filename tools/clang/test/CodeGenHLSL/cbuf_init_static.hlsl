// RUN: %dxc -E main -T ps_6_0 %s

float4 a;

static float4 m = {a};

float4 main() : SV_Target {
    return m;
}