// RUN: %dxc /Tps_6_0 /Emain %s | FileCheck %s
// github issue #1684
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
// CHECK: call void  @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)

struct VSOUT_StdPN
{
    sample float3 normal : NORMAL;
};

struct VSOUT_StdPNT
{
    sample float3 normal : NORMAL;
};

struct VSOUT
{
    VSOUT_StdPNT standard;      // CRASH
    // VSOUT_StdPN standard;      // OK
};

void foo(in VSOUT_StdPN psin)
{
}

float4 bar(VSOUT psin)
{
    foo(psin.standard);

    return 0.0f;
}

float4 main( VSOUT psin ) : SV_Target0
{
    float4 p = bar( psin );
    
    return float4(0,0,0,0);
}