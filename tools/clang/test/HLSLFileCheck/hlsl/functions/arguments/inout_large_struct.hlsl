// RUN: %dxc -T lib_6_3 %s | FileCheck %s

//
// Runtime data type that we use to associate with ray data
// This is alloca-d in the caller and then is passed around
// This also gets saved/restored in the context storage
//
struct RuntimeData
{
    uint64_t    m_pointerToPerThreadState;
    uint        m_rayIndex;
    uint        m_rayIdX;
    uint        m_rayIdY;
    uint        m_dimX;
    uint        m_dimY;


    float       m_rayTMin;
    float       m_rayTCurrent;
    uint        m_rayFlags;
    float       m_worldRayOrigin[3];
    float       m_worldRayDirection[3];
    float       m_objectRayOrigin[3];
    float       m_objectRayDirection[3];
    float       m_objectToWorld[12];
    float       m_worldToObject[12];
    
    uint        m_primitiveIndex;
    uint        m_instanceIndex;
    uint        m_instanceID;
    uint        m_hitKind;
    uint        m_shaderRecordOffset;
    
    // Pending hit values - accessed in anyHit and intersection shaders before a hit has been committed
    float       m_pendingRayTCurrent;
    uint        m_pendingPrimitiveIndex;
    uint        m_pendingInstanceIndex;
    uint        m_pendingInstanceID;
    uint        m_pendingHitKind;
    uint        m_pendingShaderRecordOffset; 
    
    int         m_groupIndex; 
    int         m_anyhitResult;
    int         m_anyhitStateId;// Originally temporary. We needed to avoid resource usage
                                // in ReportHit() because of linking issues so weset the value here first. 
                                // May be worth retaining to cache the value when fetching the intersection 
                                // stateId (fetch them both at once). 
    
    int         m_payloadOffset;            
    int         m_committedAttrOffset;      
    int         m_pendingAttrOffset;   
};

// CHECK: AccessJustOneField
// CHECK-NEXT: entry:
// CHECK-NEXT:  %0 = getelementptr inbounds %struct.RuntimeData, %struct.RuntimeData* %rd, i32 0, i32 1
// CHECK-NEXT:  %1 = load i32, i32* %0, align 4
// CHECK-NEXT:  %mul = mul i32 %1, 255
// CHECK-NEXT:  %conv = uitofp i32 %mul to float
// CHECK-NEXT:  %2 = insertelement <4 x float> undef, float %conv, i64 0
// CHECK-NEXT:  %3 = insertelement <4 x float> %2, float 1.000000e+00, i64 1

export
float4 AccessJustOneField(inout RuntimeData rd)
{

   return float4(rd.m_rayIndex * 255, 1, 0, 0);
}

export
void __XdxrCommitHit(
    inout RuntimeData runtimeData)
{
    runtimeData.m_rayTCurrent         = runtimeData.m_pendingRayTCurrent;
    runtimeData.m_shaderRecordOffset  = runtimeData.m_pendingShaderRecordOffset;
    runtimeData.m_primitiveIndex      = runtimeData.m_pendingPrimitiveIndex;
    runtimeData.m_instanceIndex       = runtimeData.m_pendingInstanceIndex;
    runtimeData.m_instanceID          = runtimeData.m_pendingInstanceID;
    runtimeData.m_hitKind             = runtimeData.m_pendingHitKind;  
    
    int pendingAttrOffset = runtimeData.m_pendingAttrOffset;
    runtimeData.m_pendingAttrOffset = runtimeData.m_committedAttrOffset;
    runtimeData.m_committedAttrOffset = pendingAttrOffset;
}

cbuffer Bla : register(b0)
{
   int g_defeatOptimizer;
};

float4 main() : SV_Target
{
   RuntimeData rd;
   rd.m_rayIndex = 0;
   rd.m_rayTCurrent = 0;
   rd.m_pendingRayTCurrent = 1;

   if (g_defeatOptimizer)
      __XdxrCommitHit(rd);

   return AccessJustOneField(rd);
}
