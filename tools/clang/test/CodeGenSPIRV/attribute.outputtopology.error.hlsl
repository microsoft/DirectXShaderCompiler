// RUN: not %dxc -T hs_6_0 -E Hull -spirv %s 2>&1 | FileCheck %s

struct ControlPoint { float4 position : POSITION; }; 
struct HullPatchOut {
    float edge [3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

HullPatchOut HullConst (InputPatch<ControlPoint,3> v) { return (HullPatchOut)0; }

//CHECK: error: hull entry point must have a valid outputtopology attribute

[domain("tri")]
[partitioning("fractional_odd")]
[patchconstantfunc("HullConst")]
[outputcontrolpoints(3)]
ControlPoint Hull (InputPatch<ControlPoint,3> v, uint id : SV_OutputControlPointID) { return v[id]; }
