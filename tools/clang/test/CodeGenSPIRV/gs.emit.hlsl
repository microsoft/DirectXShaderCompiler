// Run: %dxc -T gs_6_0 -E main

struct GsInnerOut {
    float2 bar  : BAR;
};

struct GsPerVertexOut {
    float4 pos  : SV_Position;
    float3 foo  : FOO;
    GsInnerOut s;
};

// CHECK: [[null:%\d+]] = OpConstantNull %GsPerVertexOut

[maxvertexcount(2)]
void main(in    line float2 foo[2] : FOO,
          in    line float4 pos[2] : SV_Position,
          inout      LineStream<GsPerVertexOut> outData)
{
// CHECK:            %src_main = OpFunction %void None
// CHECK:            %bb_entry = OpLabel

// CHECK-NEXT:         %vertex = OpVariable %_ptr_Function_GsPerVertexOut Function
    GsPerVertexOut vertex;
// CHECK-NEXT:                   OpStore %vertex [[null]]
    vertex = (GsPerVertexOut)0;

// Write back to stage output variables
// CHECK-NEXT: [[vertex:%\d+]] = OpLoad %GsPerVertexOut %vertex
// CHECK-NEXT:    [[pos:%\d+]] = OpCompositeExtract %v4float [[vertex]] 0
// CHECK-NEXT:                   OpStore %gl_Position [[pos]]
// CHECK-NEXT:    [[foo:%\d+]] = OpCompositeExtract %v3float [[vertex]] 1
// CHECK-NEXT:                   OpStore %out_var_FOO [[foo]]
// CHECK-NEXT:      [[s:%\d+]] = OpCompositeExtract %GsInnerOut [[vertex]] 2
// CHECK-NEXT:    [[bar:%\d+]] = OpCompositeExtract %v2float [[s]] 0
// CHECK-NEXT:                   OpStore %out_var_BAR [[bar]]
// CHECK-NEXT:                   OpEmitVertex

    outData.Append(vertex);

// Write back to stage output variables
// CHECK-NEXT: [[vertex:%\d+]] = OpLoad %GsPerVertexOut %vertex
// CHECK-NEXT:    [[pos:%\d+]] = OpCompositeExtract %v4float [[vertex]] 0
// CHECK-NEXT:                   OpStore %gl_Position [[pos]]
// CHECK-NEXT:    [[foo:%\d+]] = OpCompositeExtract %v3float [[vertex]] 1
// CHECK-NEXT:                   OpStore %out_var_FOO [[foo]]
// CHECK-NEXT:      [[s:%\d+]] = OpCompositeExtract %GsInnerOut [[vertex]] 2
// CHECK-NEXT:    [[bar:%\d+]] = OpCompositeExtract %v2float [[s]] 0
// CHECK-NEXT:                   OpStore %out_var_BAR [[bar]]
// CHECK-NEXT:                   OpEmitVertex
    outData.Append(vertex);

// CHECK-NEXT:                   OpEndPrimitive
    outData.RestartStrip();

// CHECK-NEXT:                   OpReturn
}
