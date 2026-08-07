// RUN: %dxc -HV 2021 -spirv -T cs_6_8 -E main -O0 -fspv-target-env=vulkan1.3 %s -Fo %t.spv

struct Bindings {
  const static uint Mask = 0;
};

const static uint SessionDSIndex = 0;
[[vk::binding(Bindings::Mask, SessionDSIndex)]] RWTexture2DArray<float> gMask;

struct Trigger {
  void accumulate() {}
};

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {}
