// RUN: %dxc -T ps_6_2 -E main -fspv-reflect -fcgl  %s -spirv | FileCheck %s

// Make sure the same decoration is not applied twice.
//
// CHECK:     OpDecorateString %gl_FragCoord UserSemantic "SV_POSITION"
// CHECK-NOT: OpDecorateString %gl_FragCoord UserSemantic "SV_POSITION"

float4 main(float4 pix_pos : SV_POSITION, float4 pix_pos2: SV_POSITION): SV_Target {
  return 0;
}
