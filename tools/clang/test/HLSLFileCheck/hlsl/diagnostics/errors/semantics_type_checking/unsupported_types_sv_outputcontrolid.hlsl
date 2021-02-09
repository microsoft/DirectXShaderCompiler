// RUN: %dxc -E main -T hs_6_2 -DTY=uint %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T hs_6_2 -DTY=float2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T hs_6_2 -DTY=float3 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T hs_6_2 -DTY=float4 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T hs_6_2 -DTY=int2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T hs_6_2 -DTY=int2x2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T hs_6_2 -DTY=bool2 %s | FileCheck %s -check-prefix=CHK_ERR

// CHK_NO_ERR: define void @main
// CHK_ERR: error: invalid type used for 'SV_OutputControlPointID' semantic


struct MatStruct {
  int2 uv : TEXCOORD0;
  float3x4  m_ObjectToWorld : TEXCOORD1;
};

struct Output {
  float edges[3] : SV_TessFactor;
  float inside : SV_InsideTessFactor;
};

Output Patch(InputPatch<MatStruct, 3> inputs)
{
  Output ret;
  int i = inputs[0].uv.x;
  int j = inputs[0].uv.y;

  ret.edges[0] = inputs[0].m_ObjectToWorld[0][0];
  ret.edges[1] = inputs[0].m_ObjectToWorld[0][1];
  ret.edges[2] = inputs[0].m_ObjectToWorld[0][2];
  ret.inside = 1.0f;
  return ret;
}



[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("Patch")]
[outputcontrolpoints(3)]
[shader("hull")]
float4 main(InputPatch<MatStruct, 3> input, TY id : SV_OutputControlPointID) : SV_Position
{
  return 0;
}