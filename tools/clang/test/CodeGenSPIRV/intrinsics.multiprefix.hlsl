// Run: %dxc -T ps_6_5 -E main

StructuredBuffer<uint4> g_mask;

uint main(uint input : ATTR0) : SV_Target {
  uint4 mask = g_mask[0];

  uint res = uint4(0, 0, 0, 0);
// CHECK: 10:10: error: WaveMultiPrefixBitAnd intrinsic function unimplemented
  res += WaveMultiPrefixBitAnd(input, mask);
// CHECK: 12:10: error: WaveMultiPrefixBitOr intrinsic function unimplemented
  res += WaveMultiPrefixBitOr(input, mask);
// CHECK: 14:10: error: WaveMultiPrefixBitXor intrinsic function unimplemented
  res += WaveMultiPrefixBitXor(input, mask);
// CHECK: 16:10: error: WaveMultiPrefixProduct intrinsic function unimplemented
  res += WaveMultiPrefixProduct(input, mask);
// CHECK: 18:10: error: WaveMultiPrefixSum intrinsic function unimplemented
  res += WaveMultiPrefixSum(input, mask);
// CHECK: 20:12: error: WaveMultiPrefixCountBits intrinsic function unimplemented
  res.x += WaveMultiPrefixCountBits((input.x == 1), mask);

  return res;
}
