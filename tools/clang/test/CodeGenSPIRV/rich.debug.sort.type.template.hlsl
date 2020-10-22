// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK: [[dbg_info_none:%\d+]] = OpExtInst %void [[ext:%\d+]] DebugInfoNone
// CHECK: [[ty:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite {{%\d+}} Class {{%\d+}} 0 0 {{%\d+}} {{%\d+}} [[dbg_info_none]]
// CHECK: [[param:%\d+]] = OpExtInst %void [[ext]] DebugTypeTemplateParameter {{%\d+}} {{%\d+}} [[dbg_info_none]]
// CHECK: OpExtInst %void [[ext]] DebugTypeTemplate [[ty]] [[param]]

Texture2D<float4> tex : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target0
{
  return tex.Load(int3(pos.xy, 0));
}
