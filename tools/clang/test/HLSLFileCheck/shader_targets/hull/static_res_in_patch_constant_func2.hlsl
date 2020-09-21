// RUN: %dxc -E main -T hs_6_0  %s 2>&1 | FileCheck %s

// Make sure @sf is restored to original value in patch constant function.
// CHECK:define void @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@XZ"() {
// CHECK:store float 3.000000e+00, float* @sf, align 4
// CHECK-NEXT:call void @dx.op.storePatchConstant.f32
// CHECK-NEXT:call void @dx.op.storePatchConstant.f32
// CHECK-NEXT:call void @dx.op.storePatchConstant.f32
// CHECK-NEXT:load float, float* @sf, align 4
// CHECK-NEXT:call void @dx.op.storePatchConstant.f32
struct HSPerPatchData
{
	float	edges[3] : SV_TessFactor;
	float	inside   : SV_InsideTessFactor;
};

static float sf = 3;

HSPerPatchData HSPerPatchFunc()
{
  HSPerPatchData d;

  d.edges[0] = -5;
  d.edges[1] = -6;
  d.edges[2] = -7;
  d.inside = sf;

  return d;
}



// hull per-control point shader
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HSPerPatchFunc")]
[outputcontrolpoints(3)]
void main( const uint id : SV_OutputControlPointID )
{
  sf = 0;
}