// RUN: dxc -T cs_6_0 -HV 2018 -E main -fspv-target-env=vulkan1.1 t.hlsl -ast-dump

struct S {
    float1x1 val;
    bool res;
};

RWStructuredBuffer<S> values;

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    values[id.x].res = all(WaveActiveAllEqual(values[id.x].val));
}
