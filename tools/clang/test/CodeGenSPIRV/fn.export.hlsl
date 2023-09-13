// RUN: %dxc -T lib_6_3 -fspv-target-env=universal1.5

// CHECK: OpCapability Shader
// CHECK: OpCapability Linkage
RWBuffer< float4 > output : register(u1);

// CHECK: OpDecorate %main LinkageAttributes "main" Export
// CHECK: %main = OpFunction %int None
export int main(inout float4 color) {
  output[0] = color;
  return 1;
}
