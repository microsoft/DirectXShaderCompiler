// Run: %dxc -T ds_6_0 -E BezierEvalDS

// CHECK: OpEntryPoint TessellationEvaluation %BezierEvalDS "BezierEvalDS" {{%\w+}} {{%\w+}} {{%\w+}} {{%\w+}} %gl_Position

// CHECK: OpDecorate %gl_Position BuiltIn Position

// CHECK: %gl_Position = OpVariable %_ptr_Output_v4float Output


struct HS_CONSTANT_DATA_OUTPUT
{
  float Edges[4]        : SV_TessFactor;
  float Inside[2]       : SV_InsideTessFactor;
};

// Output control point (output of hull shader)
struct BEZIER_CONTROL_POINT
{
  float3 vPosition	: BEZIERPOS;
};

// The domain shader outputs
struct DS_OUTPUT
{
  float4 vPosition  : SV_POSITION;
};

[domain("quad")]
DS_OUTPUT BezierEvalDS( HS_CONSTANT_DATA_OUTPUT input, 
                        float2 UV : SV_DomainLocation,
                        const OutputPatch<BEZIER_CONTROL_POINT, 16> bezpatch )
{
  DS_OUTPUT Output;
  return Output;
}
