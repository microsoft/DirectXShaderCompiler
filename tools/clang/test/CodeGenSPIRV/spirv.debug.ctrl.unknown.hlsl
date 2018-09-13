// Run: %dxc -T ps_6_1 -E main -fspv-debug=t

float4 main(uint val : A) : SV_Target {
  uint a = reversebits(val);
  return a;
}

// CHECK: unknown SPIR-V debug info control parameter: t
