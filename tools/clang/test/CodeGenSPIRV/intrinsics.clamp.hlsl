// Run: %dxc -T vs_6_0 -E main

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {

// CHECK:        [[int_x:%\d+]] = OpLoad %int %int_x
// CHECK-NEXT: [[int_min:%\d+]] = OpLoad %int %int_min
// CHECK-NEXT: [[int_max:%\d+]] = OpLoad %int %int_max
// CHECK-NEXT:[[temp_max:%\d+]] = OpExtInst %int [[glsl]] SMax [[int_x]] [[int_min]]
// CHECK-NEXT:         {{%\d+}} = OpExtInst %int [[glsl]] SMin [[temp_max]] [[int_max]]
  int int_x, int_min, int_max;
  int int_result = clamp(int_x, int_min, int_max);

// CHECK:        [[uint2_x:%\d+]] = OpLoad %v2uint %uint2_x
// CHECK-NEXT: [[uint2_min:%\d+]] = OpLoad %v2uint %uint2_min
// CHECK-NEXT: [[uint2_max:%\d+]] = OpLoad %v2uint %uint2_max
// CHECK-NEXT:  [[temp_max:%\d+]] = OpExtInst %v2uint [[glsl]] UMax [[uint2_x]] [[uint2_min]]
// CHECK-NEXT:           {{%\d+}} = OpExtInst %v2uint [[glsl]] UMin [[temp_max]] [[uint2_max]]
  uint2 uint2_x, uint2_min, uint2_max;
  uint2 uint2_result = clamp(uint2_x, uint2_min, uint2_max);

// CHECK:        [[float3_x:%\d+]] = OpLoad %v3float %float3_x
// CHECK-NEXT: [[float3_min:%\d+]] = OpLoad %v3float %float3_min
// CHECK-NEXT: [[float3_max:%\d+]] = OpLoad %v3float %float3_max
// CHECK-NEXT:   [[temp_max:%\d+]] = OpExtInst %v3float [[glsl]] FMax [[float3_x]] [[float3_min]]
// CHECK-NEXT:            {{%\d+}} = OpExtInst %v3float [[glsl]] FMin [[temp_max]] [[float3_max]]
  float3 float3_x, float3_min, float3_max;
  float3 float3_result = clamp(float3_x, float3_min, float3_max);

// CHECK:             [[xMat:%\d+]] = OpLoad %mat2v3float %float2x3_x
// CHECK-NEXT:      [[minMat:%\d+]] = OpLoad %mat2v3float %float2x3_min
// CHECK-NEXT:      [[maxMat:%\d+]] = OpLoad %mat2v3float %float2x3_max
// CHECK-NEXT:    [[xMatRow0:%\d+]] = OpCompositeExtract %v3float [[xMat]] 0
// CHECK-NEXT:  [[minMatRow0:%\d+]] = OpCompositeExtract %v3float [[minMat]] 0
// CHECK-NEXT:  [[maxMatRow0:%\d+]] = OpCompositeExtract %v3float [[maxMat]] 0
// CHECK-NEXT: [[tempMaxRow0:%\d+]] = OpExtInst %v3float [[glsl]] FMax [[xMatRow0]] [[minMatRow0]]
// CHECK-NEXT:  [[resultRow0:%\d+]] = OpExtInst %v3float [[glsl]] FMin [[tempMaxRow0]] [[maxMatRow0]]
// CHECK-NEXT:    [[xMatRow1:%\d+]] = OpCompositeExtract %v3float [[xMat]] 1
// CHECK-NEXT:  [[minMatRow1:%\d+]] = OpCompositeExtract %v3float [[minMat]] 1
// CHECK-NEXT:  [[maxMatRow1:%\d+]] = OpCompositeExtract %v3float [[maxMat]] 1
// CHECK-NEXT: [[tempMaxRow1:%\d+]] = OpExtInst %v3float [[glsl]] FMax [[xMatRow1]] [[minMatRow1]]
// CHECK-NEXT:  [[resultRow1:%\d+]] = OpExtInst %v3float [[glsl]] FMin [[tempMaxRow1]] [[maxMatRow1]]
// CHECK-NEXT:             {{%\d+}} = OpCompositeConstruct %mat2v3float [[resultRow0]] [[resultRow1]]
  float2x3 float2x3_x, float2x3_min, float2x3_max;
  float2x3 float2x3_result = clamp(float2x3_x, float2x3_min, float2x3_max);
}
