// RUN: %dxc -T hs_6_0 -E Hull -spirv -fcgl  %s | FileCheck %s

struct ControlPoint { float4 position : POSITION; };

struct PatchData {
    float edge [3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

PatchData HullConst () { return (PatchData)0; }

// CHECK: OpEntryPoint TessellationControl %Hull "Hull" %in_var_POSITION %gl_InvocationID %out_var_POSITION %gl_TessLevelOuter %gl_TessLevelInner
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HullConst")]
// CHECK: OpExecutionMode %Hull OutputVertices 0
[outputcontrolpoints(0)]
ControlPoint Hull (InputPatch<ControlPoint,3> v, uint id : SV_OutputControlPointID) { return v[id]; }
