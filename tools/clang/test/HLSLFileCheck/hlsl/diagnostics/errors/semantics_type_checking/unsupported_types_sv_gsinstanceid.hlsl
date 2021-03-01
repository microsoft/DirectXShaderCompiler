// RUN: %dxc -E main -T gs_6_2 -DTY=uint %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T gs_6_2 -DTY=float2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T gs_6_2 -DTY=int2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T gs_6_2 -DTY=int2x2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T gs_6_2 -DTY=float %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T gs_6_2 -DTY=bool2 %s | FileCheck %s -check-prefix=CHK_ERR

// CHK_NO_ERR: define void @main
// CHK_ERR: error: invalid type used for 'SV_GSInstanceID' semantic


struct VSOut {
  float2 uv : TEXCOORD0;
};

struct VSOutGSIn {
  float3 posSize : POSSIZE;
  float4 clr : COLOR;
};

struct VSOutGSArrayIn {
  float3 posSize : POSSIZE;
  float2 clr[2] : COLOR;
};

struct VSOutGSMatIn {
  float3 posSize : POSSIZE;
  float2x2 clr[2] : COLOR;
};

cbuffer b : register(b0) {
  float2 invViewportSize;
};

// geometry shader that outputs 3 vertices from a point
[maxvertexcount(3)] 
[instance(24)]
void main(point VSOutGSIn points[1], inout TriangleStream<VSOut> stream, TY InstanceID : SV_GSInstanceID) {  
}
