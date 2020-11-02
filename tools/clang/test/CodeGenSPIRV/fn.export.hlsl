// Run: %dxc -T lib_6_3 -Od

RWBuffer< float4 > output : register(u1);

// CHECK: OpEntryPoint GLCompute %dummyEntry "dummyEntry"
// CHECK: %main = OpFunction %int None
export int main(inout float4 color) {
  output[0] = color;
  return 1;
}
