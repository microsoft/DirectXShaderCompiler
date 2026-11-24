// RUN: %dxc -spirv -T cs_6_0 -E main %s -Fo %t.spv

static const uint Binding = 0;
[[vk::binding(Binding, 0)]] RWTexture2D<float> output;
void g() {}
[numthreads(1, 1, 1)] void main() { g(); output[uint2(0, 0)] = 1.0; }
