// Run: %dxc -T gs_6_0 -E main

// Note that the 'Position' semantic can be both GSOut and GSVIn. Both are tested below.

// CHECK: OpEntryPoint Geometry %main "main" %gl_Position %gl_Position_0 %out_var_COLOR0 %out_var_TEXCOORD0

// CHECK: OpDecorate %gl_Position BuiltIn Position
// CHECK: OpDecorate %gl_Position_0 BuiltIn Position

// CHECK: %gl_Position = OpVariable %_ptr_Input__arr_v4float_uint_3 Input
// CHECK: %gl_Position_0 = OpVariable %_ptr_Output_v4float Output

struct GS_OUT
{ 
  float4 position : SV_POSITION;
  float4 color    : COLOR0;
  float2 uv       : TEXCOORD0;
};

[maxvertexcount(3)] 
void main(triangle float4 vertexPos[3] : SV_POSITION, inout TriangleStream <GS_OUT> outstream)
{
}
