// RUN: %dxc %s -T lib_6_9 -DPRESET_LOCAL_RAYDESC=1 | FileCheck %s --check-prefix LOCAL
// RUN: %dxc %s -T lib_6_9 -DPRESET_CBUFFER_RAYDESC_NORQ=1 | FileCheck %s --check-prefix CBUFFER

// cbank load lowering bug:
// COM: %dxc %s -T lib_6_9 -DPRESET_CBUFFER_RAYDESC=1 | FileCheck %s --check-prefix CBUFFER_RQ

#if PRESET_LOCAL_RAYDESC
// Single local RayDesc with using ops

#define RD0 1
// #define RD1 1
#define HIT1 1
#define HIT2 1
#define HIT3 1
#define HIT4 1

// LOCAL: %{{[^ ]+}} = call %dx.types.HitObject @dx.op.hitObject_MakeMiss(i32 265, i32 0, i32 0, float 0.000000e+00, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00, float 5.000000e+00, float 6.000000e+00, float 7.000000e+00)  ; HitObject_MakeMiss(RayFlags,MissShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax)
// LOCAL: %{{[^ ]+}} = call %dx.types.HitObject @dx.op.hitObject_TraceRay.struct.PerRayData(i32 262, %dx.types.Handle %{{[^ ]+}}, i32 256, i32 255, i32 0, i32 0, i32 0, float 0.000000e+00, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00, float 5.000000e+00, float 6.000000e+00, float 7.000000e+00, %struct.PerRayData* nonnull %{{[^ ]+}})  ; HitObject_TraceRay(accelerationStructure,rayFlags,instanceInclusionMask,rayContributionToHitGroupIndex,multiplierForGeometryContributionToHitGroupIndex,missShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax,payload)
// LOCAL: call void @dx.op.rayQuery_TraceRayInline(i32 179, i32 %{{[^ ]+}}, %dx.types.Handle %{{[^ ]+}}, i32 0, i32 255, float 0.000000e+00, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00, float 5.000000e+00, float 6.000000e+00, float 7.000000e+00)  ; RayQuery_TraceRayInline(rayQueryHandle,accelerationStructure,rayFlags,instanceInclusionMask,origin_X,origin_Y,origin_Z,tMin,direction_X,direction_Y,direction_Z,tMax)
// LOCAL: call void @dx.op.traceRay.struct.PerRayData(i32 157, %dx.types.Handle %{{[^ ]+}}, i32 256, i32 255, i32 0, i32 0, i32 0, float 0.000000e+00, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00, float 5.000000e+00, float 6.000000e+00, float 7.000000e+00, %struct.PerRayData* nonnull %{{[^ ]+}})  ; TraceRay(AccelerationStructure,RayFlags,InstanceInclusionMask,RayContributionToHitGroupIndex,MultiplierForGeometryContributionToShaderIndex,MissShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax,payload)

#endif


#if PRESET_CBUFFER_RAYDESC_NORQ
// RayDesc served from cbuffer

// #define RD0 1
#define RD1 1
#define HIT1 1
#define HIT2 1
// #define HIT3 1
#define HIT4 1

// CBUFFER: %[[LD00:[^ ]+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %[[CBH:[^ ]+]], i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CBUFFER: %[[RD0O0:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD00]], 0
// CBUFFER: %[[RD0O1:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD00]], 1
// CBUFFER: %[[RD0O2:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD00]], 2
// CBUFFER: %[[RD0MIN:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD00]], 3
// CBUFFER: %[[LD01:[^ ]+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %[[CBH]], i32 1)  ; CBufferLoadLegacy(handle,regIndex)
// CBUFFER: %[[RD0D0:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD01]], 0
// CBUFFER: %[[RD0D1:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD01]], 1
// CBUFFER: %[[RD0D2:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD01]], 2
// CBUFFER: %[[RD0MAX:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD01]], 3
// CBUFFER: %{{[^ ]+}} = call %dx.types.HitObject @dx.op.hitObject_MakeMiss(i32 265, i32 0, i32 0, float %[[RD0O0]], float %[[RD0O1]], float %[[RD0O2]], float %[[RD0MIN]], float %[[RD0D0]], float %[[RD0D1]], float %[[RD0D2]], float %[[RD0MAX]])  ; HitObject_MakeMiss(RayFlags,MissShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax)

// CBUFFER: %[[LD10:[^ ]+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %[[CBH]], i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CBUFFER: %[[RD1O0:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD10]], 0
// CBUFFER: %[[RD1O1:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD10]], 1
// CBUFFER: %[[RD1O2:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD10]], 2
// CBUFFER: %[[RD1MIN:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD10]], 3
// CBUFFER: %[[LD11:[^ ]+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %[[CBH]], i32 1)  ; CBufferLoadLegacy(handle,regIndex)
// CBUFFER: %[[RD1D0:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD11]], 0
// CBUFFER: %[[RD1D1:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD11]], 1
// CBUFFER: %[[RD1D2:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD11]], 2
// CBUFFER: %[[RD1MAX:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD11]], 3
// CBUFFER: %{{[^ ]+}} = call %dx.types.HitObject @dx.op.hitObject_TraceRay.struct.PerRayData(i32 262, %dx.types.Handle %{{[^ ]+}}, i32 256, i32 255, i32 0, i32 0, i32 0, float %[[RD1O0]], float %[[RD1O1]], float %[[RD1O2]], float %[[RD1MIN]], float %[[RD1D0]], float %[[RD1D1]], float %[[RD1D2]], float %[[RD1MAX]], %struct.PerRayData* nonnull %{{[^ ]+}})  ; HitObject_TraceRay(accelerationStructure,rayFlags,instanceInclusionMask,rayContributionToHitGroupIndex,multiplierForGeometryContributionToHitGroupIndex,missShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax,payload)

// CBUFFER: %[[LD20:[^ ]+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %[[CBH]], i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CBUFFER: %[[RD2O0:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD20]], 0
// CBUFFER: %[[RD2O1:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD20]], 1
// CBUFFER: %[[RD2O2:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD20]], 2
// CBUFFER: %[[RD2MIN:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD20]], 3
// CBUFFER: %[[LD21:[^ ]+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %[[CBH]], i32 1)  ; CBufferLoadLegacy(handle,regIndex)
// CBUFFER: %[[RD2D0:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD21]], 0
// CBUFFER: %[[RD2D1:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD21]], 1
// CBUFFER: %[[RD2D2:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD21]], 2
// CBUFFER: %[[RD2MAX:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[LD21]], 3
// CBUFFER: call void @dx.op.traceRay.struct.PerRayData(i32 157, %dx.types.Handle %{{[^ ]+}}, i32 256, i32 255, i32 0, i32 0, i32 0, float %[[RD2O0]], float %[[RD2O1]], float %[[RD2O2]], float %[[RD2MIN]], float %[[RD2D0]], float %[[RD2D1]], float %[[RD2D2]], float %[[RD2MAX]], %struct.PerRayData* nonnull %{{[^ ]+}})  ; TraceRay(AccelerationStructure,RayFlags,InstanceInclusionMask,RayContributionToHitGroupIndex,MultiplierForGeometryContributionToShaderIndex,MissShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax,payload)

#endif

#if PRESET_CBUFFER_RAYDESC
// TraceRayInline with RayDesc from cbuffer
// Lowering broken

// #define RD0 1
#define RD1 1
// #define HIT1 1
// #define HIT2 1
#define HIT3 1
// #define HIT4 1
#endif


struct[raypayload] PerRayData
{
};

RaytracingAccelerationStructure topObject : register(t0);

#if RD1
RayDesc ray;
#endif

[shader("raygeneration")]
void raygen()
{
#if RD0
    RayDesc ray = {{0, 1, 2}, 3,  {4, 5, 6}, 7};
#endif

    PerRayData payload;
#if HIT1
    dx::HitObject hit1 = dx::HitObject::MakeMiss(RAY_FLAG_NONE, 0, ray);
    dx::MaybeReorderThread(hit1);
#endif
#if HIT2
    dx::HitObject hit2 = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_TRIANGLES, 0xFF, 0, 0, 0, ray, payload);
    dx::MaybeReorderThread(hit2);
#endif
#if HIT3
    RayQuery<RAY_FLAG_NONE> rayQuery;
    rayQuery.TraceRayInline(topObject, RAY_FLAG_NONE, 0xFF, ray);
    dx::HitObject hit3 = dx::HitObject::FromRayQuery(rayQuery);
    dx::MaybeReorderThread(hit3);
#endif
#if HIT4
    TraceRay(topObject, RAY_FLAG_SKIP_TRIANGLES, 0xFF, 0, 0, 0, ray, payload);
#endif
}
