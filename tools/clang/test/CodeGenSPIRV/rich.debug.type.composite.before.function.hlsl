// Run: %dxc -T vs_6_0 -E main -fspv-debug=rich

// CHECK: [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK: [[out:%\d+]] = OpString "VS_OUTPUT"
// CHECK: [[main:%\d+]] = OpString "main"

// CHECK: [[VSOUT:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[out]]
// CHECK: [[ty:%\d+]] = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate [[VSOUT]]
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[main]] [[ty]]

struct VS_OUTPUT {
  float4 pos : SV_POSITION;
  float4 color : COLOR;
};

VS_OUTPUT main(float4 pos : POSITION,
               float4 color : COLOR) {
  VS_OUTPUT vout;
  vout.pos = pos;
  vout.color = color;
  return vout;
}
