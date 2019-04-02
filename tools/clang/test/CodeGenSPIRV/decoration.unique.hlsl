// Run: %dxc -T ps_6_2 -E main -fspv-reflect

// Make sure the same decoration is not applied twice.
//
// CHECK:     OpDecorateStringGOOGLE %gl_FragCoord HlslSemanticGOOGLE "SV_POSITION"
// CHECK-NOT: OpDecorateStringGOOGLE %gl_FragCoord HlslSemanticGOOGLE "SV_POSITION"

float4 main(float4 pix_pos : SV_POSITION, float4 pix_pos2: SV_POSITION): SV_Target {
  return 0;
}
