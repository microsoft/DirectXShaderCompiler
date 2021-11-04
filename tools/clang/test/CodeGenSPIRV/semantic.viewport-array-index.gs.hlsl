// RUN: %dxc -T gs_6_0 -E main

// CHECK:      OpCapability MultiViewport

// CHECK:      OpEntryPoint Geometry %main "main"
// CHECK-SAME: %in_var_SV_ViewportArrayIndex
// CHECK-SAME: %gl_ViewportIndex

// CHECK:      OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex
// CHECK:      OpDecorate %in_var_SV_ViewportArrayIndex Location 0

// CHECK:      %in_var_SV_ViewportArrayIndex = OpVariable %_ptr_Input__arr_uint_uint_2 Input
// CHECK:      %gl_ViewportIndex = OpVariable %_ptr_Output_uint Output

// GS per-vertex input
struct GsVIn {
  uint index : SV_ViewportArrayIndex;
};

// GS per-vertex output
struct GsVOut {
  uint index : SV_ViewportArrayIndex;
};

[maxvertexcount(2)]
void main(in    line GsVIn              inData[2],
          inout      LineStream<GsVOut> outData) {

    GsVOut vertex;
    vertex = (GsVOut)0;
    outData.Append(vertex);

    outData.RestartStrip();
}
