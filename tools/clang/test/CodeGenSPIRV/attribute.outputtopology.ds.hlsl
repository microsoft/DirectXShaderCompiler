// Run: %dxc -T ds_6_0 -E BezierEvalDS

// Make sure outputtopology is emitted for Domain shaders too.

#define MAX_POINTS 4

// Output patch constant data (output of hull shader)
struct HS_CONSTANT_DATA_OUTPUT {
  float Edges[4]        : SV_TessFactor;
  float Inside[2]       : SV_InsideTessFactor;
};
struct BEZIER_CONTROL_POINT {
  float3 vPosition	: BEZIERPOS;
};
struct DS_OUTPUT {
  float4 vPosition  : SV_POSITION;
};

// CHECK: OpExecutionMode %BezierEvalDS VertexOrderCw
[outputtopology("triangle_cw")]
[domain("quad")]
DS_OUTPUT BezierEvalDS( HS_CONSTANT_DATA_OUTPUT input,
                        float2 UV : SV_DomainLocation,
                        const OutputPatch<BEZIER_CONTROL_POINT, MAX_POINTS> bezpatch )
{
  DS_OUTPUT Output;
  return Output;
}
