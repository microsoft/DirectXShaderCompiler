// Run: %dxc -T ps_6_0 -E main

SamplerState ss : register(s2);

Texture1D        <float>  t1;

void main() {
  float x = 0.5;

  float lod1 = t1.CalculateLevelOfDetailUnclamped(ss, x);
}

// CHECK: :10:19: error: no equivalent for CalculateLevelOfDetailUnclamped intrinsic method in Vulkan
