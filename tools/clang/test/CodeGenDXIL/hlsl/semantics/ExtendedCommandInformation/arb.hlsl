// RUN: %dxc -E vs_main -T vs_6_8 %s | FileCheck %s --check-prefix=VS
// RUN: %dxc -E hs_main -T hs_6_8 %s | FileCheck %s
// RUN: %dxc -E ds_main -T ds_6_8 %s | FileCheck %s
// RUN: %dxc -E gs_main -T gs_6_8 %s | FileCheck %s
// RUN: %dxc -E ps_main -T ps_6_8 %s | FileCheck %s --check-prefix=PS

// Make sure not in signature for VS input.
// VS: ; Input signature:
// VS: ; Name                 Index   Mask Register SysValue  Format   Used
// VS: ; no parameters
// Make sure in output signature is not sysvalue
// VS: ; Output signature:
// VS: ; Name                 Index   Mask Register SysValue  Format   Used
// VS: ; -------------------- ----- ------ -------- -------- ------- ------
// VS: ; SV_Position              0   xyzw        0      POS   float   xyzw
// VS: ; SV_StartInstanceLocation     0   x           1     NONE    uint   x
// VS: ; SV_IndirectCommandIndex     0    y          1     NONE    uint    y

// VS: %[[Index:.+]] = call i32 @dx.op.indirectCommandIndex.i32(i32 258)
// VS: %[[Location:.+]] = call i32 @dx.op.startInstanceLocation.i32(i32 257)
// VS: call void @dx.op.storeOutput.i32(i32 5, i32 1, i32 0, i8 0, i32 %[[Location]])
// VS: call void @dx.op.storeOutput.i32(i32 5, i32 2, i32 0, i8 0, i32 %[[Index]])


float4 vs_main(inout uint loc : SV_StartInstanceLocation
		   , inout uint index : SV_IndirectCommandIndex) : SV_Position {
  return 0;
}


struct PSSceneIn {
  uint loc : SV_StartInstanceLocation;
  uint index : SV_IndirectCommandIndex;
  float4 pos : SV_Position;
};


// Make sure input is not sysvalue
// CHECK: ; Input signature:
// CHECK: ; Name                 Index   Mask Register SysValue  Format   Used
// CHECK: ; -------------------- ----- ------ -------- -------- ------- ------
// CHECK: ; SV_StartInstanceLocation     0   x           0     NONE    uint   x
// CHECK: ; SV_IndirectCommandIndex     0    y          0     NONE    uint    y
// CHECK: ; SV_Position              0   xyzw        1      POS   float   xyzw

// Make sure in output signature is not sysvalue
// CHECK: ; Output signature:
// CHECK: ; Name                 Index   Mask Register SysValue  Format   Used
// CHECK: ; -------------------- ----- ------ -------- -------- ------- ------
// CHECK: ; SV_StartInstanceLocation     0   x           0     NONE    uint   x
// CHECK: ; SV_IndirectCommandIndex     0    y          0     NONE    uint    y
// CHECK: ; SV_Position              0   xyzw        1      POS   float   xyzw

// CHECK: %[[Location:.+]] = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 0, i32 {{.*}})
// CHECK: %[[Index:.+]] = call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 0, i8 0, i32 {{.*}})
// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 %[[Location]])  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 1, i32 0, i8 0, i32 %[[Index]])


struct HSPerVertexData {
  // This is just the original vertex verbatim. In many real life cases this would be a
  // control point instead
  PSSceneIn v;
};


struct HSPerPatchData {
  // We at least have to specify tess factors per patch
  // As we're tesselating triangles, there will be 4 tess factors
  // In real life case this might contain face normal, for example
  float edges[3] : SV_TessFactor;
  float inside : SV_InsideTessFactor;
};

HSPerPatchData HSPerPatchFunc( const InputPatch< PSSceneIn, 3 > points, OutputPatch<HSPerVertexData, 3> outp)
{
    HSPerPatchData d;

    d.edges[ 0 ] = 1;
    d.edges[ 1 ] = 1;
    d.edges[ 2 ] = 1;
    d.inside = 1;

    return d;
}

// hull per-control point shader
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HSPerPatchFunc")]
[outputcontrolpoints(3)]
HSPerVertexData hs_main( const uint id : SV_OutputControlPointID,
                               const InputPatch< PSSceneIn, 3 > points)
{
    HSPerVertexData v;

    // Just forward the vertex
    v.v = points[ id ];

	return v;
}


[domain("tri")] PSSceneIn ds_main(const float3 bary
                               : SV_DomainLocation,
                                 const OutputPatch<HSPerVertexData, 3> patch,
                                 const HSPerPatchData perPatchData) {
  PSSceneIn v;

  // Compute interpolated coordinates
  v.pos = patch[0].v.pos * bary.x + patch[1].v.pos * bary.y + patch[2].v.pos * bary.z + perPatchData.edges[1];
  v.loc = patch[0].v.loc;
  v.index = patch[0].v.index;
  return v;
}


[maxvertexcount(3)]
[instance(24)]
void gs_main(InputPatch<PSSceneIn, 2>points, inout PointStream<PSSceneIn> stream) {

  stream.Append(points[0]);

  stream.RestartStrip();
}


// Make sure input is not sysvalue
// PS: ; Input signature:
// PS: ;
// PS: ; Name                 Index   Mask Register SysValue  Format   Used
// PS: ; -------------------- ----- ------ -------- -------- ------- ------
// PS: ; SV_StartInstanceLocation     0   x           0     NONE    uint   x
// PS: ; SV_IndirectCommandIndex     0    y          0     NONE    uint    y

// PS: %[[Index:.+]] = call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 0, i8 0, i32 undef)
// PS: %[[Location:.+]] = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 0, i32 undef)
// PS: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 %[[Location]])
// PS: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 1, i32 %[[Index]])

uint2 ps_main(uint loc : SV_StartInstanceLocation
		   ,  uint index : SV_IndirectCommandIndex) : SV_Target {
  return uint2(loc,index);		   
}