// Run: %dxc -T ps_6_0 -E main

Texture2DMS       <float> myTexture;

void main() {
  float2 ret = myTexture.GetSamplePosition(2);
}

// CHECK: :6:26: error: no equivalent for GetSamplePosition intrinsic method in Vulkan
