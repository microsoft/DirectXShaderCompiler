// RUN: %dxc /Tps_6_0 /Emain %s | FileCheck %s
// internally reported bug
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.i32(i32 5, i32 3, i32 0, i8 0, i32 0)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)

#define min16float3                             float3
#define min16float4                             float4

struct MatParams
{
    min16float4 albedo;
    min16float  roughness;
    min16float  ao;
};

struct Gbuffer
{
    min16float4 rt0 : SV_Target0;
    min16float4 rt1 : SV_Target1;
    float4 rt2 : SV_Target2;
    uint rt3 : SV_Target3;
};

Gbuffer packGbuffer( in MatParams matParams, in float2 sampleUV )
{
    Gbuffer gb;

    gb.rt0 = min16float4( matParams.albedo.xyz,  0);
    gb.rt1 = min16float4( 0,0,0, matParams.ao );
    gb.rt2 = float4( matParams.roughness, 0,0,0 );
    gb.rt3 = 0;

    return gb;
}

struct VSOUT_StdP
{
    sample float4          hclip           : SV_Position;
    sample float3          worldPosition   : WORLD_POSITION;
};

struct VSOUT_StdPN
{
    VSOUT_StdP      position;
    sample min16float3 normal : NORMAL;
    sample min16float4 tangent : TANGENT;
};

// bitcast %struct.VSOUT_StdPNT* %standard.i to %struct.VSOUT_StdPN*
struct VSOUT_StdPNT
{
    VSOUT_StdP      position;
    sample min16float3 normal : NORMAL;
    sample min16float4 tangent : TANGENT;
};

min16float3 getNormal(in VSOUT_StdPN vsout)
{
    return vsout.normal;
}

#define VSOUT_POSITION_DATA VSOUT_StdPNT


struct VSOutput
{
    VSOUT_POSITION_DATA    standard;
    sample min16float4     colour      : COLOR;
    sample float4          uv0uv1      : TEXCOORD4;
};

typedef MatParams      TerrainOutput;

MatParams terrainCommon(VSOutput input)
{
    min16float3 N = getNormal( input.standard );    
    TerrainOutput material = (TerrainOutput)0;    
    return material;
}

Gbuffer main(VSOutput i)
{
    MatParams material = terrainCommon(i);
    Gbuffer o = packGbuffer(material, i.standard.position.hclip.xy);    
    return o;        
}
