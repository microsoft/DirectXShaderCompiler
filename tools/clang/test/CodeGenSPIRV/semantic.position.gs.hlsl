// Run: %dxc -T gs_6_0 -E main

// TODO: 'Position' semantics for GSVIn.

// CHECK: OpEntryPoint Geometry %main "main" {{%\w+}} %gl_Position {{%\w+}} {{%\w+}}

// CHECK: OpDecorate %gl_Position BuiltIn Position

// CHECK: %gl_Position = OpVariable %_ptr_Output_v4float Output

struct GS_OUT
{
  float4 position : SV_POSITION;
  float4 color    : COLOR0;
  float2 uv       : TEXCOORD0;
};

[maxvertexcount(3)] 
void main(triangle float4 vid[3] : VertexID, inout TriangleStream <GS_OUT> outstream)
{
}
