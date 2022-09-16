// RUN: %dxc -T vs_6_0 -E main -fspv-target-env=vulkan1.1

// CHECK: OpCapability ShaderClockKHR
// CHECK: {{%\d+}} = OpExtInstImport "GLSL.std.450"
// CHECK: {{%\d+}} = OpExtInstImport "SPV_AMD_shader_trinary_minmax"

struct SInstanceData {
  float4x3 VisualToWorld;
  float4 Output;
};

struct VS_INPUT	{
  float3 Position : POSITION;
  SInstanceData	InstanceData : TEXCOORD4;
};

[[vk::ext_capability(5055)]]
[[vk::ext_extension("SPV_KHR_shader_clock")]]
[[vk::ext_instruction(/* OpReadClockKHR */ 5056)]]
uint64_t ReadClock(uint scope);

[[vk::ext_instruction(/* Sin*/ 13, "GLSL.std.450")]]
float4 spv_sin(float4 v);

[[vk::ext_instruction(/* FMin3AMD */ 1, "SPV_AMD_shader_trinary_minmax")]]
float FMin3AMD(float x, float y, float z);

float4 main(const VS_INPUT v) : SV_Position {
	SInstanceData	I = v.InstanceData;
  uint64_t clock;
// CHECK: {{%\d+}} = OpExtInst %v4float {{%\d+}} Sin {{%\d+}}
  I.Output = spv_sin(v.InstanceData.Output);
// CHECK: {{%\d+}} = OpReadClockKHR %ulong %uint_1
  clock = ReadClock(vk::DeviceScope);
// CHECK: {{%\d+}} = OpExtInst %float {{%\d+}} FMin3AMD {{%\d+}} {{%\d+}} {{%\d+}}
  I.Output.w = FMin3AMD(I.Output.z, I.Output.y, I.Output.z);

  return I.Output;
}
