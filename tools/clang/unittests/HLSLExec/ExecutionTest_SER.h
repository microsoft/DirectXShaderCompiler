//===--------- ExecutionTest_SER.h - SER Execution Tests -------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ExecutionTest_SER.h                                                       //
// Copyright (C) Nvidia Corporation. All rights reserved.                    //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This file contains the execution tests for SER.                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

TEST_F(ExecutionTest, SERScalarGetterTest) {
  // SER: Test basic function of HitObject getters.
  static const char *pShader = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 windowSize;
    int rayFlags;    
};

struct[raypayload] PerRayData
{
    VALTYPE value : read(anyhit,closesthit,miss,caller) : write(anyhit,miss,closesthit);
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

RWStructuredBuffer<VALTYPE> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

RayDesc ComputeRay()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    float2 d = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;
    RayDesc ray;
    ray.Origin = sceneConstants.eye.xyz;
    ray.Direction = normalize(d.x*sceneConstants.U.xyz + d.y*sceneConstants.V.xyz + sceneConstants.W.xyz);
    ray.TMin = 0;
    ray.TMax = 1e18;

    return ray;
}

[shader("raygeneration")]
void raygen()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;
    int id = 2 * (launchIndex.x + launchIndex.y * launchDim.x);

    RayDesc ray = ComputeRay();

    // Fetch reference value
    PerRayData refPayload;
    TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, refPayload);
    testBuffer[id] = refPayload.value;
    
    PerRayData serPayload;
    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, serPayload);
    dx::MaybeReorderThread(hitObject);
    VALTYPE serVal = hitObject.SER_GET_SCALAR();
    testBuffer[id + 1] = serVal;
}

float getFloatZero() { return 0.0f; }
int getIntZero() { return 0; }

[shader("miss")]
void miss(inout PerRayData payload)
{
    payload.value = MISS_GET_SCALAR(); 
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    // UNUSED
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.value = HIT_GET_SCALAR(); 
}
)";

  CComPtr<ID3D12Device> pDevice;
  bool bSM_6_9_Supported = CreateDevice(&pDevice, D3D_SHADER_MODEL_6_9, false);
  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(L"SERTest requires shader model 6.9+ but no "
                               L"supported device was found.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }
  bool bDXRSupported =
      bSM_6_9_Supported && DoesDeviceSupportRayTracing(pDevice);

  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support SM 6.9.");
  }
  if (!bDXRSupported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support DXR.");
  }

  // Initialize test data.
  const int windowSize = 64;

  if (!bDXRSupported)
    return;

  WEX::Logging::Log::Comment(L"==== DXR lib_6_9 with SER");

  // RayTMin
  {
    std::vector<int> testData(windowSize * windowSize * 2, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=float",
                      L"-DHIT_GET_SCALAR=RayTMin",
                      L"-DMISS_GET_SCALAR=RayTMin",
                      L"-DSER_GET_SCALAR=GetRayTMin"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    for (int id = 0; id < testData.size(); id += 2) {
      float *resArray = (float *)(testData.data() + id);
      float refVal = resArray[0];
      float serVal = resArray[1];
      const bool passRayTMin = CompareFloatEpsilon(serVal, refVal, 0.0008f);
      if (!passRayTMin) {
        VERIFY_IS_TRUE(passRayTMin);
        WEX::Logging::Log::Comment(L"HitObject::GetRayTMin() FAILED");
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetRayTMin() PASSED");
  }

  // RayTCurrent
  {
    std::vector<int> testData(windowSize * windowSize * 2, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=float",
                      L"-DHIT_GET_SCALAR=RayTCurrent",
                      L"-DMISS_GET_SCALAR=RayTCurrent",
                      L"-DSER_GET_SCALAR=GetRayTCurrent"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    for (int id = 0; id < testData.size(); id += 2) {
      float *resArray = (float *)(testData.data() + id);
      float refVal = resArray[0];
      float serVal = resArray[1];
      const bool passRayTCurrent = CompareFloatEpsilon(serVal, refVal, 0.0008f);
      if (!passRayTCurrent) {
        VERIFY_IS_TRUE(passRayTCurrent);
        WEX::Logging::Log::Comment(L"HitObject::GetRayTCurrent() FAILED");
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetRayTCurrent() PASSED");
  }

  // RayFlags
  {
    std::vector<int> testData(windowSize * windowSize * 2, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=RayFlags",
                      L"-DMISS_GET_SCALAR=RayFlags",
                      L"-DSER_GET_SCALAR=GetRayFlags"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    for (int id = 0; id < testData.size(); id += 2) {
      const int refVal = testData[id];
      const int serVal = testData[id + 1];
      if (refVal != serVal) {
        VERIFY_ARE_EQUAL(refVal, serVal);
        WEX::Logging::Log::Comment(L"HitObject::GetRayFlags() FAILED");
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetRayFlags() PASSED");
  }

  // HitKind
  {
    std::vector<int> testData(windowSize * windowSize * 2, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=HitKind",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetHitKind"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    for (int id = 0; id < testData.size(); id += 2) {
      const int refVal = testData[id];
      const int serVal = testData[id + 1];
      if (refVal != serVal) {
        VERIFY_ARE_EQUAL(refVal, serVal);
        WEX::Logging::Log::Comment(L"HitObject::GetHitKind() FAILED");
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetHitKind() PASSED");
  }

  // GeometryIndex
  {
    std::vector<int> testData(windowSize * windowSize * 2, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=GeometryIndex",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetGeometryIndex"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    for (int id = 0; id < testData.size(); id += 2) {
      const int refVal = testData[id];
      const int serVal = testData[id + 1];
      if (refVal != serVal) {
        VERIFY_ARE_EQUAL(refVal, serVal);
        WEX::Logging::Log::Comment(L"HitObject::GetGeometryIndex() FAILED");
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetGeometryIndex() PASSED");
  }

  // InstanceIndex
  {
    std::vector<int> testData(windowSize * windowSize * 2, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=InstanceIndex",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetInstanceIndex"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    for (int id = 0; id < testData.size(); id += 2) {
      const int refVal = testData[id];
      const int serVal = testData[id + 1];
      if (refVal != serVal) {
        VERIFY_ARE_EQUAL(refVal, serVal);
        WEX::Logging::Log::Comment(L"HitObject::GetInstanceIndex() FAILED");
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetInstanceIndex() PASSED");
  }

  // InstanceID
  {
    std::vector<int> testData(windowSize * windowSize * 2, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=InstanceID",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetInstanceID"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    for (int id = 0; id < testData.size(); id += 2) {
      const int refVal = testData[id];
      const int serVal = testData[id + 1];
      if (refVal != serVal) {
        VERIFY_ARE_EQUAL(refVal, serVal);
        WEX::Logging::Log::Comment(L"HitObject::GetInstanceID() FAILED");
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetInstanceID() PASSED");
  }

  // PrimitiveIndex
  {
    std::vector<int> testData(windowSize * windowSize * 2, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=PrimitiveIndex",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetPrimitiveIndex"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    for (int id = 0; id < testData.size(); id += 2) {
      const int refVal = testData[id];
      const int serVal = testData[id + 1];
      if (refVal != serVal) {
        VERIFY_ARE_EQUAL(refVal, serVal);
        WEX::Logging::Log::Comment(L"HitObject::GetPrimitiveIndex() FAILED");
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetPrimitiveIndex() PASSED");
  }
}

TEST_F(ExecutionTest, SERVectorGetterTest) {
  // SER: Test basic function of HitObject getters.
  static const char *pShader = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 windowSize;
    int rayFlags;    
};

struct[raypayload] PerRayData
{
    float3 value : read(caller) : write(miss,closesthit);
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

RWStructuredBuffer<float> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

RayDesc ComputeRay()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    float2 d = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;
    RayDesc ray;
    ray.Origin = sceneConstants.eye.xyz;
    ray.Direction = normalize(d.x*sceneConstants.U.xyz + d.y*sceneConstants.V.xyz + sceneConstants.W.xyz);
    ray.TMin = 0;
    ray.TMax = 1e18;

    return ray;
}

[shader("raygeneration")]
void raygen()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;
    int id = 6 * (launchIndex.x + launchIndex.y * launchDim.x);

    RayDesc ray = ComputeRay();

    // Fetch reference value
    PerRayData refPayload;
    TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, refPayload);
    testBuffer[id] = refPayload.value.x;
    testBuffer[id + 2] = refPayload.value.y;
    testBuffer[id + 4] = refPayload.value.z;
    
    PerRayData serPayload;
    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, serPayload);
    dx::MaybeReorderThread(hitObject);
    float3 serVal = hitObject.SER_GET_VECTOR();
    testBuffer[id + 1] = serVal.x;
    testBuffer[id + 3] = serVal.y;
    testBuffer[id + 5] = serVal.z;
}

float3 getVecZero() { return 0.0f; }

[shader("miss")]
void miss(inout PerRayData payload)
{
    payload.value = MISS_GET_VECTOR(); 
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    // UNUSED
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.value = HIT_GET_VECTOR(); 
}
)";

  CComPtr<ID3D12Device> pDevice;
  bool bSM_6_9_Supported = CreateDevice(&pDevice, D3D_SHADER_MODEL_6_9, false);
  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(L"SERTest requires shader model 6.9+ but no "
                               L"supported device was found.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }
  bool bDXRSupported =
      bSM_6_9_Supported && DoesDeviceSupportRayTracing(pDevice);

  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support SM 6.9.");
  }
  if (!bDXRSupported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support DXR.");
  }

  // Initialize test data.
  const int windowSize = 64;

  if (!bDXRSupported)
    return;

  WEX::Logging::Log::Comment(L"==== DXR lib_6_9 with SER");

  // WorldRayOrigin
  {
    std::vector<int> testData(windowSize * windowSize * 6, 0);
    LPCWSTR args[] = {L"-HV 2021", L"-Vd", L"-DHIT_GET_VECTOR=WorldRayOrigin",
                      L"-DMISS_GET_VECTOR=WorldRayOrigin",
                      L"-DSER_GET_VECTOR=GetWorldRayOrigin"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/,
               3 /*payloadCount*/);
    for (int id = 0; id < testData.size(); id += 6) {
      float *resArray = (float *)(testData.data() + id);
      float refX = resArray[0];
      float serX = resArray[1];
      float refY = resArray[2];
      float serY = resArray[3];
      float refZ = resArray[4];
      float serZ = resArray[5];
      const bool passX = CompareFloatEpsilon(serX, refX, 0.0008f);
      const bool passY = CompareFloatEpsilon(serY, refY, 0.0008f);
      const bool passZ = CompareFloatEpsilon(serZ, refZ, 0.0008f);
      if (!passX || !passY || !passZ) {
        VERIFY_ARE_EQUAL(serX, refX);
        VERIFY_ARE_EQUAL(serY, refY);
        VERIFY_ARE_EQUAL(serZ, refZ);
        WEX::Logging::Log::Comment(L"HitObject::GetWorldRayOrigin() FAILED");
        break;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetWorldRayOrigin() PASSED");
  }

  // WorldRayDirection
  {
    std::vector<int> testData(windowSize * windowSize * 6, 0);
    LPCWSTR args[] = {L"-HV 2021", L"-Vd",
                      L"-DHIT_GET_VECTOR=WorldRayDirection",
                      L"-DMISS_GET_VECTOR=WorldRayDirection",
                      L"-DSER_GET_VECTOR=GetWorldRayDirection"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/,
               3 /*payloadCount*/);
    for (int id = 0; id < testData.size(); id += 6) {
      float *resArray = (float *)(testData.data() + id);
      float refX = resArray[0];
      float serX = resArray[1];
      float refY = resArray[2];
      float serY = resArray[3];
      float refZ = resArray[4];
      float serZ = resArray[5];
      const bool passX = CompareFloatEpsilon(serX, refX, 0.0008f);
      const bool passY = CompareFloatEpsilon(serY, refY, 0.0008f);
      const bool passZ = CompareFloatEpsilon(serZ, refZ, 0.0008f);
      if (!passX || !passY || !passZ) {
        VERIFY_ARE_EQUAL(serX, refX);
        VERIFY_ARE_EQUAL(serY, refY);
        VERIFY_ARE_EQUAL(serZ, refZ);
        WEX::Logging::Log::Comment(L"HitObject::GetWorldRayDirection() FAILED");
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetWorldRayDirection() PASSED");
  }

  // ObjectRayOrigin
  {
    std::vector<int> testData(windowSize * windowSize * 6, 0);
    LPCWSTR args[] = {L"-HV 2021", L"-Vd", L"-DHIT_GET_VECTOR=ObjectRayOrigin",
                      L"-DMISS_GET_VECTOR=WorldRayOrigin",
                      L"-DSER_GET_VECTOR=GetObjectRayOrigin"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/,
               3 /*payloadCount*/);
    for (int id = 0; id < testData.size(); id += 6) {
      float *resArray = (float *)(testData.data() + id);
      float refX = resArray[0];
      float serX = resArray[1];
      float refY = resArray[2];
      float serY = resArray[3];
      float refZ = resArray[4];
      float serZ = resArray[5];
      const bool passX = CompareFloatEpsilon(serX, refX, 0.0008f);
      const bool passY = CompareFloatEpsilon(serY, refY, 0.0008f);
      const bool passZ = CompareFloatEpsilon(serZ, refZ, 0.0008f);
      if (!passX || !passY || !passZ) {
        VERIFY_ARE_EQUAL(serX, refX);
        VERIFY_ARE_EQUAL(serY, refY);
        VERIFY_ARE_EQUAL(serZ, refZ);
        WEX::Logging::Log::Comment(L"HitObject::GetObjectRayOrigin() FAILED");
        break;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetObjectRayOrigin() PASSED");
  }

  // ObjectRayDirection
  {
    std::vector<int> testData(windowSize * windowSize * 6, 0);
    LPCWSTR args[] = {L"-HV 2021", L"-Vd",
                      L"-DHIT_GET_VECTOR=ObjectRayDirection",
                      L"-DMISS_GET_VECTOR=WorldRayDirection",
                      L"-DSER_GET_VECTOR=GetObjectRayDirection"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/,
               3 /*payloadCount*/);
    for (int id = 0; id < testData.size(); id += 6) {
      float *resArray = (float *)(testData.data() + id);
      float refX = resArray[0];
      float serX = resArray[1];
      float refY = resArray[2];
      float serY = resArray[3];
      float refZ = resArray[4];
      float serZ = resArray[5];
      const bool passX = CompareFloatEpsilon(serX, refX, 0.0008f);
      const bool passY = CompareFloatEpsilon(serY, refY, 0.0008f);
      const bool passZ = CompareFloatEpsilon(serZ, refZ, 0.0008f);
      if (!passX || !passY || !passZ) {
        VERIFY_ARE_EQUAL(serX, refX);
        VERIFY_ARE_EQUAL(serY, refY);
        VERIFY_ARE_EQUAL(serZ, refZ);
        WEX::Logging::Log::Comment(
            L"HitObject::GetObjectRayDirection() FAILED");
        break;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetObjectRayDirection() PASSED");
  }
}

TEST_F(ExecutionTest, SERMatrixGetterTest) {
  // SER: Test basic function of HitObject getters.
  static const char *pShader = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 windowSize;
    int rayFlags;
};

struct[raypayload] PerRayData
{
    matrix<float,ROWS,COLS> value : read(caller) : write(miss,closesthit);
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

RWStructuredBuffer<float> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

RayDesc ComputeRay()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    float2 d = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;
    RayDesc ray;
    ray.Origin = sceneConstants.eye.xyz;
    ray.Direction = normalize(d.x*sceneConstants.U.xyz + d.y*sceneConstants.V.xyz + sceneConstants.W.xyz);
    ray.TMin = 0;
    ray.TMax = 1e18;

    return ray;
}

[shader("raygeneration")]
void raygen()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;
    int id = 2 * ROWS * COLS * (launchIndex.x + launchIndex.y * launchDim.x);

    RayDesc ray = ComputeRay();

    // Fetch reference value
    PerRayData refPayload;
    TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, refPayload);
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        testBuffer[id + 2 * (r * COLS + c)] = refPayload.value[r][c];
      }
    }

    PerRayData serPayload;
    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, serPayload);
    dx::MaybeReorderThread(hitObject);
    matrix<float,ROWS,COLS> serVal = hitObject.SER_GET_MATRIX();
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        testBuffer[1 + id + 2 * (r * COLS + c)] = serVal[r][c];
      }
    }
}

matrix<float,ROWS,COLS> getMatIdentity() {
  matrix<float,ROWS,COLS> mat = 0;
  mat[0][0] = 1.f;
  mat[1][1] = 1.f;
  mat[2][2] = 1.f;
  return mat;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    payload.value = MISS_GET_MATRIX();
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    // UNUSED
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.value = HIT_GET_MATRIX();
}
)";

  CComPtr<ID3D12Device> pDevice;
  bool bSM_6_9_Supported = CreateDevice(&pDevice, D3D_SHADER_MODEL_6_9, false);
  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(L"SERTest requires shader model 6.9+ but no "
                               L"supported device was found.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }
  bool bDXRSupported =
      bSM_6_9_Supported && DoesDeviceSupportRayTracing(pDevice);

  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support SM 6.9.");
  }
  if (!bDXRSupported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support DXR.");
  }

  // Initialize test data.
  const int windowSize = 64;

  if (!bDXRSupported)
    return;

  WEX::Logging::Log::Comment(L"==== DXR lib_6_9 with SER");

  // WorldToObject3x4
  {
    std::vector<int> testData(windowSize * windowSize * 24, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DHIT_GET_MATRIX=WorldToObject3x4",
                      L"-DMISS_GET_MATRIX=getMatIdentity",
                      L"-DSER_GET_MATRIX=GetWorldToObject3x4",
                      L"-DROWS=3",
                      L"-DCOLS=4"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/,
               12 /*payloadCount*/);
    const int ROWS = 3;
    const int COLS = 4;
    for (int id = 0; id < testData.size(); id += 24) {
      float *resArray = (float *)(testData.data() + id);
      for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
          int refIdx = 2 * (r * COLS + c);
          float ref = resArray[refIdx];
          float ser = resArray[1 + refIdx];
          if (!CompareFloatEpsilon(ser, ref, 0.0008f)) {
            VERIFY_ARE_EQUAL(ser, ref);
          }
        }
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetWorldToObject3x4() PASSED");
  }

  // WorldToObject4x3
  {
    const int ROWS = 4;
    const int COLS = 3;
    std::vector<int> testData(windowSize * windowSize * 2 * ROWS * COLS, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DHIT_GET_MATRIX=WorldToObject4x3",
                      L"-DMISS_GET_MATRIX=getMatIdentity",
                      L"-DSER_GET_MATRIX=GetWorldToObject4x3",
                      L"-DROWS=4",
                      L"-DCOLS=3"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/,
               12 /*payloadCount*/);
    for (int id = 0; id < testData.size(); id += 2 * ROWS * COLS) {
      float *resArray = (float *)(testData.data() + id);
      for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
          int refIdx = 2 * (r * COLS + c);
          float ref = resArray[refIdx];
          float ser = resArray[1 + refIdx];
          if (!CompareFloatEpsilon(ser, ref, 0.0008f)) {
            VERIFY_ARE_EQUAL(ser, ref);
          }
        }
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetWorldToObject4x3() PASSED");
  }

  // ObjectToWorld3x4
  {
    std::vector<int> testData(windowSize * windowSize * 24, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DHIT_GET_MATRIX=ObjectToWorld3x4",
                      L"-DMISS_GET_MATRIX=getMatIdentity",
                      L"-DSER_GET_MATRIX=GetObjectToWorld3x4",
                      L"-DROWS=3",
                      L"-DCOLS=4"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/,
               12 /*payloadCount*/);
    const int ROWS = 3;
    const int COLS = 4;
    for (int id = 0; id < testData.size(); id += 24) {
      float *resArray = (float *)(testData.data() + id);
      for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
          int refIdx = 2 * (r * COLS + c);
          float ref = resArray[refIdx];
          float ser = resArray[1 + refIdx];
          if (!CompareFloatEpsilon(ser, ref, 0.0008f)) {
            VERIFY_ARE_EQUAL(ser, ref);
          }
        }
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetObjectToWorld3x4() PASSED");
  }

  // ObjectToWorld4x3
  {
    std::vector<int> testData(windowSize * windowSize * 24, 0);
    LPCWSTR args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DHIT_GET_MATRIX=ObjectToWorld4x3",
                      L"-DMISS_GET_MATRIX=getMatIdentity",
                      L"-DSER_GET_MATRIX=GetObjectToWorld4x3",
                      L"-DROWS=4",
                      L"-DCOLS=3"};
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/,
               12 /*payloadCount*/);
    const int ROWS = 4;
    const int COLS = 3;
    for (int id = 0; id < testData.size(); id += 24) {
      float *resArray = (float *)(testData.data() + id);
      for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
          int refIdx = 2 * (r * COLS + c);
          float ref = resArray[refIdx];
          float ser = resArray[1 + refIdx];
          if (!CompareFloatEpsilon(ser, ref, 0.0008f)) {
            VERIFY_ARE_EQUAL(ser, ref);
            WEX::Logging::Log::Comment(
                L"HitObject::GetObjectToWorld4x3() FAILED");
            break;
          }
        }
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetObjectToWorld4x3() PASSED");
  }
}

TEST_F(ExecutionTest, SERBasicTest) {
  // SER: Test basic functionality.
  static const char *pShader = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 windowSize;
    int rayFlags;    
};

struct[raypayload] PerRayData
{
    uint visited : read(anyhit,closesthit,miss,caller) : write(anyhit,miss,closesthit,caller);
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

RWStructuredBuffer<int> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

RayDesc ComputeRay()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    float2 d = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;
    RayDesc ray;
    ray.Origin = sceneConstants.eye.xyz;
    ray.Direction = normalize(d.x*sceneConstants.U.xyz + d.y*sceneConstants.V.xyz + sceneConstants.W.xyz);
    ray.TMin = 0;
    ray.TMax = 1e18;

    return ray;
}

[shader("raygeneration")]
void raygen()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    RayDesc ray = ComputeRay();

    PerRayData payload;
    payload.visited = 0;

    // SER Test
    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    dx::MaybeReorderThread(hitObject);
    dx::HitObject::Invoke(hitObject, payload);

    int id = launchIndex.x + launchIndex.y * launchDim.x;
    testBuffer[id] = payload.visited;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    payload.visited |= 2U;
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 1U;
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 4U;
}

)";

  CComPtr<ID3D12Device> pDevice;
  bool bSM_6_9_Supported = CreateDevice(&pDevice, D3D_SHADER_MODEL_6_9, false);
  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(L"SERTest requires shader model 6.9+ but no "
                               L"supported device was found.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }
  bool bDXRSupported =
      bSM_6_9_Supported && DoesDeviceSupportRayTracing(pDevice);

  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support SM 6.9.");
  }
  if (!bDXRSupported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support DXR.");
  }

  // Initialize test data.
  const int windowSize = 64;
  std::vector<int> testData(windowSize * windowSize, 0);
  LPCWSTR args[] = {L"-HV 2021", L"-Vd"};

  if (bDXRSupported) {
    WEX::Logging::Log::Comment(L"==== DXR lib_6_9 with SER");
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    std::map<int, int> histo;
    for (int val : testData) {
      ++histo[val];
    }
    VERIFY_ARE_EQUAL(histo.size(), 2);
    VERIFY_ARE_EQUAL(histo[2], 4030);
    VERIFY_ARE_EQUAL(histo[5], 66);
  }
}

TEST_F(ExecutionTest, SERRayQueryTest) {
  // Test SER RayQuery
  static const char *pShader = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 windowSize;
    int rayFlags;
};

struct[raypayload] PerRayData
{
    uint visited : read(anyhit,closesthit,miss,caller) : write(anyhit,miss,closesthit,caller);
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

RWStructuredBuffer<int> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

RayDesc ComputeRay()
{
    float2 d = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;
    RayDesc ray;
    ray.Origin = sceneConstants.eye.xyz;
    ray.Direction = normalize(d.x*sceneConstants.U.xyz + d.y*sceneConstants.V.xyz + sceneConstants.W.xyz);
    ray.TMin = 0;
    ray.TMax = 1e18;

    return ray;
}

[shader("raygeneration")]
void raygen()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    RayDesc ray = ComputeRay();

    PerRayData payload;
    payload.visited = 0;

    // Template parameter set at runtime before compilation
    RayQuery<RAY_FLAG_NONE> rayQ;

    // Funtion parameter set at runtime before compilation
    rayQ.TraceRayInline(topObject, RAY_FLAG_NONE, 0xFF, ray);

    // Storage for procedural primitive hit attributes
    Attrs attrs;
    attrs.barycentrics = float2(1, 1);

    while (rayQ.Proceed())
    {
        switch (rayQ.CandidateType())
        {

            case CANDIDATE_NON_OPAQUE_TRIANGLE:
            {
                // The system has already determined that the candidate would be the closest
                // hit so far in the ray extents
                rayQ.CommitNonOpaqueTriangleHit();
            }
        }
    }

#if 0
    switch (rayQ.CommittedStatus())
    {
        case COMMITTED_TRIANGLE_HIT:
        {
            if (rayQ.CommittedTriangleFrontFace())
            {
                // Hit
                payload.visited |= 4U;
            }
            break;
        }
        case COMMITTED_PROCEDURAL_PRIMITIVE_HIT:
        {
            // Unused
            break;
        }
        case COMMITTED_NOTHING:
        {
            // Miss
            payload.visited |= 2U;
            break;
        }
    }
#else
    dx::HitObject hit;
    if (rayQ.CommittedStatus() == COMMITTED_NOTHING)
    {
        hit = dx::HitObject::MakeMiss(RAY_FLAG_NONE, 0, ray);
    }
    else
    {
        hit = dx::HitObject::FromRayQuery(rayQ);
    }
    dx::MaybeReorderThread(hit);
    dx::HitObject::Invoke(hit, payload);
#endif

    int id = launchIndex.x + launchIndex.y * launchDim.x;
    testBuffer[id] = payload.visited;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    payload.visited |= 2U;
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 1U;
    AcceptHitAndEndSearch();
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 4U;
}

)";

  CComPtr<ID3D12Device> pDevice;
  bool bSM_6_9_Supported = CreateDevice(&pDevice, D3D_SHADER_MODEL_6_9, false);
  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(L"SERRayQueryTest requires shader model 6.9+ "
                               L"but no supported device was found.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }
  bool bDXRSupported =
      bSM_6_9_Supported && DoesDeviceSupportRayTracing(pDevice);

  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(
        L"SERRayQueryTest skipped, device does not support SM 6.9.");
  }
  if (!bDXRSupported) {
    WEX::Logging::Log::Comment(
        L"SERRayQueryTest skipped, device does not support DXR.");
  }

  // Initialize test data.
  const int windowSize = 64;
  std::vector<int> testData(windowSize * windowSize, 0);
  LPCWSTR args[] = {L"-HV 2021", L"-Vd"};

  if (bDXRSupported) {
    WEX::Logging::Log::Comment(L"==== DXR lib_6_9 with SER");
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, false /*useIS*/);
    std::map<int, int> histo;
    for (int val : testData) {
      ++histo[val];
    }
    VERIFY_ARE_EQUAL(histo.size(), 2);
    VERIFY_ARE_EQUAL(histo[0], 66);
    VERIFY_ARE_EQUAL(histo[2], 4030);
  }
}

TEST_F(ExecutionTest, SERIntersectionTest) {
  // Test SER with Intersection and procedural geometry
  static const char *pShader = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 windowSize;
    int rayFlags;    
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

struct[raypayload] PerRayData
{
    uint visited : read(anyhit, closesthit, miss, caller) : write(anyhit, miss, closesthit, caller);
};

RWStructuredBuffer<int> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

RayDesc ComputeRay()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float2 d = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;
    RayDesc ray;
    ray.Origin = sceneConstants.eye.xyz;
    ray.Direction = normalize(d.x * sceneConstants.U.xyz + d.y * sceneConstants.V.xyz + sceneConstants.W.xyz);
    ray.TMin = 0;
    ray.TMax = 1e18;

    return ray;
}

[shader("raygeneration")]
void raygen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    RayDesc ray = ComputeRay();

    PerRayData payload;
    payload.visited = 0;

#if 0
    dx::HitObject hitObject;
    TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
#else
    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    dx::MaybeReorderThread(hitObject);
    dx::HitObject::Invoke(hitObject, payload);
#endif

    int id = launchIndex.x + launchIndex.y * launchDim.x;  
    testBuffer[id] = payload.visited;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    payload.visited |= 2U;
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 1U;
    AcceptHitAndEndSearch();
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 4U;
}

[shader("intersection")]
void intersection()
{
    Attrs attrs;

    ReportHit(0.1, 0, attrs);
}

)";

  CComPtr<ID3D12Device> pDevice;
  bool bSM_6_9_Supported = CreateDevice(&pDevice, D3D_SHADER_MODEL_6_9, false);
  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(L"SERTest requires shader model 6.9+ but no "
                               L"supported device was found.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }
  bool bDXRSupported =
      bSM_6_9_Supported && DoesDeviceSupportRayTracing(pDevice);

  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support SM 6.9.");
  }
  if (!bDXRSupported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support DXR.");
  }

  // Initialize test data.
  const int windowSize = 64;
  std::vector<int> testData(windowSize * windowSize, 0);
  LPCWSTR args[] = {L"-HV 2021", L"-Vd"};

  if (bDXRSupported) {
    WEX::Logging::Log::Comment(L"==== DXR lib_6_9 with SER");
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, false /*mesh*/,
               true /*procedural geometry*/, true /*useIS*/);
    std::map<int, int> histo;
    for (int val : testData) {
      ++histo[val];
    }
    VERIFY_ARE_EQUAL(histo.size(), 2);
    VERIFY_ARE_EQUAL(histo[2], 3400);
    VERIFY_ARE_EQUAL(histo[5], 696);
  }
}

TEST_F(ExecutionTest, SERGetAttributesTest) {
  // Test SER with HitObject::GetAttributes
  static const char *pShader = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 windowSize;
    int rayFlags;    
};

struct CustomAttrs
{
    float dist;
};

struct[raypayload] PerRayData
{
    uint visited : read(anyhit, closesthit, miss, caller) : write(anyhit, miss, closesthit, caller);
};

// reordercoherent // Requires #7250
RWStructuredBuffer<int> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

RayDesc ComputeRay()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float2 d = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;
    RayDesc ray;
    ray.Origin = sceneConstants.eye.xyz;
    ray.Direction = normalize(d.x * sceneConstants.U.xyz + d.y * sceneConstants.V.xyz + sceneConstants.W.xyz);
    ray.TMin = 0;
    ray.TMax = 1e18;

    return ray;
}

[shader("raygeneration")]
void raygen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    RayDesc ray = ComputeRay();

    PerRayData payload;
    payload.visited = 0;

    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    dx::MaybeReorderThread(hitObject);
    
    // Check Attributes for hit detection.
    CustomAttrs customAttrs = hitObject.GetAttributes<CustomAttrs>();
    bool isHit = hitObject.IsHit();

    int testVal = 0;
    if (isHit) {
        if (int(floor(customAttrs.dist)) % 2 == 0)
          testVal = hitObject.GetHitKind();
    }
    else
    {
        // Use 255 to keep outside the HitKind range [0, 127] we passthru for hits.
        testVal = 255;
    }
    int id = launchIndex.x + launchIndex.y * launchDim.x;
    testBuffer[id] = testVal;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    // UNUSED
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in CustomAttrs attrs)
{
    AcceptHitAndEndSearch();
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in CustomAttrs attrs)
{
    // UNUSED
}

[shader("intersection")]
void intersection()
{
    // hitPos is intersection point with plane (base, n)
    float3 base = {0.0f,0.0f,0.5f};
    float3 n = normalize(float3(0.0f,0.5f,0.5f));
    float t = dot(n, base - ObjectRayOrigin()) / dot(n, ObjectRayDirection());
    if (t > RayTCurrent() || t < RayTMin()) {
        return;
    }
    float3 hitPos = ObjectRayOrigin() + t * ObjectRayDirection();
    float3 relHitPos = hitPos - base;
    // Encode some hit information in hitKind
    int hitKind = 0;
    if (relHitPos.y >= 0.0f)
        hitKind = 1;
    hitKind *= 2;
    if (relHitPos.x >= 0.0f)
        hitKind += 1;
    hitKind *= 2;
    if (relHitPos.z >= 0.0f)
        hitKind += 1;

    CustomAttrs attrs;
    attrs.dist = length(relHitPos);
    ReportHit(t, hitKind, attrs);
}

)";

  CComPtr<ID3D12Device> pDevice;
  bool bSM_6_9_Supported = CreateDevice(&pDevice, D3D_SHADER_MODEL_6_9, false);
  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(L"SERTest requires shader model 6.9+ but no "
                               L"supported device was found.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }
  bool bDXRSupported =
      bSM_6_9_Supported && DoesDeviceSupportRayTracing(pDevice);

  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support SM 6.9.");
  }
  if (!bDXRSupported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support DXR.");
  }

  // Initialize test data.
  const int windowSize = 64;
  std::vector<int> testData(windowSize * windowSize, 0);
  LPCWSTR args[] = {L"-HV 2021", L"-Vd"};

  if (bDXRSupported) {
    WEX::Logging::Log::Comment(L"==== DXR lib_6_9 with SER");
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, false /*mesh*/,
               true /*procedural geometry*/, true /*useIS*/);
    std::map<int, int> histo;
    for (int val : testData) {
      ++histo[val];
    }
    VERIFY_ARE_EQUAL(histo.size(), 4);
    VERIFY_ARE_EQUAL(histo[0], 328);
    VERIFY_ARE_EQUAL(histo[1], 186);
    VERIFY_ARE_EQUAL(histo[3], 182);
    VERIFY_ARE_EQUAL(histo[255], 3400);
  }
}

TEST_F(ExecutionTest, SERTraceHitMissNopTest) {
  // Test SER with conditional HitObject::TraceRay, HitObject::IsHit,
  // HitObject::IsMiss, HitObject::IsNop
  static const char *pShader = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 windowSize;
    int rayFlags;    
};

struct[raypayload] PerRayData
{
    uint visited : read(anyhit,closesthit,miss,caller) : write(anyhit,miss,closesthit,caller);
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

RWStructuredBuffer<int> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

RayDesc ComputeRay()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    float2 d = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;
    RayDesc ray;
    ray.Origin = sceneConstants.eye.xyz;
    ray.Direction = normalize(d.x*sceneConstants.U.xyz + d.y*sceneConstants.V.xyz + sceneConstants.W.xyz);
    ray.TMin = 0;
    ray.TMax = 1e18;

    return ray;
}

[shader("raygeneration")]
void raygen()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    RayDesc ray = ComputeRay();

    PerRayData payload;
    payload.visited = 0;

    // SER Test
    dx::HitObject hitObject;
    if (launchIndex.x % 2 == 0) {
      hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    }
    dx::MaybeReorderThread(hitObject);

    // Check hitObject for hit detection.
    if (hitObject.IsHit())
        payload.visited |= 4U;
    if (hitObject.IsMiss())
        payload.visited |= 2U;
    if (hitObject.IsNop())
        payload.visited |= 1U;

    dx::HitObject::Invoke(hitObject, payload);

    int id = launchIndex.x + launchIndex.y * launchDim.x;
    testBuffer[id] = payload.visited;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    payload.visited |= 16U;
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 8U;
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 32U;
}

)";

  CComPtr<ID3D12Device> pDevice;
  bool bSM_6_9_Supported = CreateDevice(&pDevice, D3D_SHADER_MODEL_6_9, false);
  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(L"SERTest requires shader model 6.9+ but no "
                               L"supported device was found.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }
  bool bDXRSupported =
      bSM_6_9_Supported && DoesDeviceSupportRayTracing(pDevice);

  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support SM 6.9.");
  }
  if (!bDXRSupported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support DXR.");
  }

  // Initialize test data.
  const int windowSize = 64;
  std::vector<int> testData(windowSize * windowSize, 0);
  LPCWSTR args[] = {L"-HV 2021", L"-Vd"};

  if (bDXRSupported) {
    WEX::Logging::Log::Comment(L"==== DXR lib_6_9 with SER");
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*mesh*/,
               false /*procedural geometry*/, false /*useIS*/);
    std::map<int, int> histo;
    for (int val : testData) {
      ++histo[val];
    }
    VERIFY_ARE_EQUAL(histo.size(), 3);
    VERIFY_ARE_EQUAL(
        histo[1],
        2048); // isNop && !isMiss && !isHit && !anyhit && !closesthit && !miss
    VERIFY_ARE_EQUAL(
        histo[18],
        2015); // !isNop && isMiss && !isHit && !anyhit && !closesthit && miss
    VERIFY_ARE_EQUAL(
        histo[44],
        33); // !isNop && !isMiss && isHit && anyhit && closesthit && !miss
  }
}

TEST_F(ExecutionTest, SERIsMissTest) {
  // Test SER with HitObject::IsMiss
  static const char *pShader = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 windowSize;
    int rayFlags;    
};

struct[raypayload] PerRayData
{
    uint visited : read(anyhit,closesthit,miss,caller) : write(anyhit,miss,closesthit,caller);
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

RWStructuredBuffer<int> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

RayDesc ComputeRay()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    float2 d = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;
    RayDesc ray;
    ray.Origin = sceneConstants.eye.xyz;
    ray.Direction = normalize(d.x*sceneConstants.U.xyz + d.y*sceneConstants.V.xyz + sceneConstants.W.xyz);
    ray.TMin = 0;
    ray.TMax = 1e18;

    return ray;
}

[shader("raygeneration")]
void raygen()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    RayDesc ray = ComputeRay();

    PerRayData payload;
    payload.visited = 0;

    // SER Test
    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    dx::MaybeReorderThread(hitObject);
    dx::HitObject::Invoke(hitObject, payload);

    // Check hitObject for hit detection.
    if (hitObject.IsMiss())
        payload.visited |= 2U;

    int id = launchIndex.x + launchIndex.y * launchDim.x;
    testBuffer[id] = payload.visited;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    // UNUSED
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 1U;
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 4U;
}

)";

  CComPtr<ID3D12Device> pDevice;
  bool bSM_6_9_Supported = CreateDevice(&pDevice, D3D_SHADER_MODEL_6_9, false);
  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(L"SERTest requires shader model 6.9+ but no "
                               L"supported device was found.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }
  bool bDXRSupported =
      bSM_6_9_Supported && DoesDeviceSupportRayTracing(pDevice);

  if (!bSM_6_9_Supported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support SM 6.9.");
  }
  if (!bDXRSupported) {
    WEX::Logging::Log::Comment(
        L"SER tests skipped, device does not support DXR.");
  }

  // Initialize test data.
  const int windowSize = 64;
  std::vector<int> testData(windowSize * windowSize, 0);
  LPCWSTR args[] = {L"-HV 2021", L"-Vd"};

  if (bDXRSupported) {
    WEX::Logging::Log::Comment(L"==== DXR lib_6_9 with SER");
    RunDXRTest(pDevice, pShader, D3D_SHADER_MODEL_6_9, args, _countof(args),
               testData, windowSize, windowSize, true /*mesh*/,
               false /*procedural geometry*/, false /*useIS*/);
    std::map<int, int> histo;
    for (int val : testData) {
      ++histo[val];
    }
    VERIFY_ARE_EQUAL(histo.size(), 2);
    VERIFY_ARE_EQUAL(histo[2], 4030);
    VERIFY_ARE_EQUAL(histo[5], 66);
  }
}
