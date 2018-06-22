// Run: %dxc -T gs_6_0 -E main -fvk-invert-y

// GS per-vertex input
struct GsVIn {
    float4 pos : SV_Position;
};

// GS per-vertex output
struct GsVOut {
    float4 pos : SV_Position;
};

[maxvertexcount(2)]
void main(in    line GsVIn              inData[2],
          inout      LineStream<GsVOut> outData) {

    GsVOut vertex;
    vertex = (GsVOut)0;
// CHECK:      [[vert:%\d+]] = OpLoad %GsVOut %vertex
// CHECK-NEXT:  [[val:%\d+]] = OpCompositeExtract %v4float [[vert]] 0
// CHECK-NEXT: [[oldY:%\d+]] = OpCompositeExtract %float [[val]] 1
// CHECK-NEXT: [[newY:%\d+]] = OpFNegate %float [[oldY]]
// CHECK-NEXT:  [[pos:%\d+]] = OpCompositeInsert %v4float [[newY]] [[val]] 1
// CHECK-NEXT:                 OpStore %gl_Position_0 [[pos]]
    outData.Append(vertex);

    outData.RestartStrip();
}

