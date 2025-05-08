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

#pragma once

TEST_F(ExecutionTest, SERScalarGetterTest) {
  // SER: Test basic function of HitObject getters.
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Initialize test data.
  const int WindowSize = 64;

  // RayTMin
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetRayTMin()");
    std::vector<int> TestData(WindowSize * WindowSize * 2, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=float",
                      L"-DHIT_GET_SCALAR=RayTMin",
                      L"-DMISS_GET_SCALAR=RayTMin",
                      L"-DSER_GET_SCALAR=GetRayTMin"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 1 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 2) {
      float *ResArray = (float *)(TestData.data() + Id);
      float RefVal = ResArray[0];
      float SerVal = ResArray[1];
      const bool PassRayTMin = CompareFloatEpsilon(SerVal, RefVal, 0.0008f);
      if (!PassRayTMin) {
        VERIFY_IS_TRUE(PassRayTMin);
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetRayTMin() PASSED");
  }

  // RayTCurrent
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetRayTCurrent()");
    std::vector<int> TestData(WindowSize * WindowSize * 2, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=float",
                      L"-DHIT_GET_SCALAR=RayTCurrent",
                      L"-DMISS_GET_SCALAR=RayTCurrent",
                      L"-DSER_GET_SCALAR=GetRayTCurrent"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 1 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 2) {
      float *ResArray = (float *)(TestData.data() + Id);
      float RefVal = ResArray[0];
      float SerVal = ResArray[1];
      const bool PassRayTCurrent = CompareFloatEpsilon(SerVal, RefVal, 0.0008f);
      if (!PassRayTCurrent) {
        VERIFY_IS_TRUE(PassRayTCurrent);
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetRayTCurrent() PASSED");
  }

  // RayFlags
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetRayFlags()");
    std::vector<int> TestData(WindowSize * WindowSize * 2, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=RayFlags",
                      L"-DMISS_GET_SCALAR=RayFlags",
                      L"-DSER_GET_SCALAR=GetRayFlags"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 1 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 2) {
      const int RefVal = TestData[Id];
      const int SerVal = TestData[Id + 1];
      if (RefVal != SerVal) {
        VERIFY_ARE_EQUAL(RefVal, SerVal);
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetRayFlags() PASSED");
  }

  // HitKind
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetHitKind()");
    std::vector<int> TestData(WindowSize * WindowSize * 2, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=HitKind",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetHitKind"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 1 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 2) {
      const int RefVal = TestData[Id];
      const int SerVal = TestData[Id + 1];
      if (RefVal != SerVal) {
        VERIFY_ARE_EQUAL(RefVal, SerVal);
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetHitKind() PASSED");
  }

  // GeometryIndex
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetGeometryIndex()");
    std::vector<int> TestData(WindowSize * WindowSize * 2, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=GeometryIndex",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetGeometryIndex"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 1 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 2) {
      const int RefVal = TestData[Id];
      const int SerVal = TestData[Id + 1];
      if (RefVal != SerVal) {
        VERIFY_ARE_EQUAL(RefVal, SerVal);
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetGeometryIndex() PASSED");
  }

  // InstanceIndex
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetInstanceIndex()");
    std::vector<int> TestData(WindowSize * WindowSize * 2, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=InstanceIndex",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetInstanceIndex"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 1 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 2) {
      const int RefVal = TestData[Id];
      const int SerVal = TestData[Id + 1];
      if (RefVal != SerVal) {
        VERIFY_ARE_EQUAL(RefVal, SerVal);
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetInstanceIndex() PASSED");
  }

  // InstanceID
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetInstanceID()");
    std::vector<int> TestData(WindowSize * WindowSize * 2, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=InstanceID",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetInstanceID"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 1 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 2) {
      const int RefVal = TestData[Id];
      const int SerVal = TestData[Id + 1];
      if (RefVal != SerVal) {
        VERIFY_ARE_EQUAL(RefVal, SerVal);
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetInstanceID() PASSED");
  }

  // PrimitiveIndex
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetPrimitiveIndex()");
    std::vector<int> TestData(WindowSize * WindowSize * 2, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DVALTYPE=uint",
                      L"-DHIT_GET_SCALAR=PrimitiveIndex",
                      L"-DMISS_GET_SCALAR=getIntZero",
                      L"-DSER_GET_SCALAR=GetPrimitiveIndex"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 1 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 2) {
      const int RefVal = TestData[Id];
      const int SerVal = TestData[Id + 1];
      if (RefVal != SerVal) {
        VERIFY_ARE_EQUAL(RefVal, SerVal);
        return;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetPrimitiveIndex() PASSED");
  }
}

TEST_F(ExecutionTest, SERVectorGetterTest) {
  // SER: Test basic function of HitObject getters.
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Initialize test data.
  const int WindowSize = 64;

  // WorldRayOrigin
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetWorldRayOrigin()");
    std::vector<int> TestData(WindowSize * WindowSize * 6, 0);
    LPCWSTR Args[] = {L"-HV 2021", L"-Vd", L"-DHIT_GET_VECTOR=WorldRayOrigin",
                      L"-DMISS_GET_VECTOR=WorldRayOrigin",
                      L"-DSER_GET_VECTOR=GetWorldRayOrigin"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 3 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 6) {
      float *ResArray = (float *)(TestData.data() + Id);
      float RefX = ResArray[0];
      float SerX = ResArray[1];
      float RefY = ResArray[2];
      float SerY = ResArray[3];
      float RefZ = ResArray[4];
      float SerZ = ResArray[5];
      const bool PassX = CompareFloatEpsilon(SerX, RefX, 0.0008f);
      const bool PassY = CompareFloatEpsilon(SerY, RefY, 0.0008f);
      const bool PassZ = CompareFloatEpsilon(SerZ, RefZ, 0.0008f);
      if (!PassX || !PassY || !PassZ) {
        VERIFY_ARE_EQUAL(SerX, RefX);
        VERIFY_ARE_EQUAL(SerY, RefY);
        VERIFY_ARE_EQUAL(SerZ, RefZ);
        break;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetWorldRayOrigin() PASSED");
  }

  // WorldRayDirection
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetWorldRayDirection()");
    std::vector<int> TestData(WindowSize * WindowSize * 6, 0);
    LPCWSTR Args[] = {L"-HV 2021", L"-Vd",
                      L"-DHIT_GET_VECTOR=WorldRayDirection",
                      L"-DMISS_GET_VECTOR=WorldRayDirection",
                      L"-DSER_GET_VECTOR=GetWorldRayDirection"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 3 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 6) {
      float *ResArray = (float *)(TestData.data() + Id);
      float RefX = ResArray[0];
      float SerX = ResArray[1];
      float RefY = ResArray[2];
      float SerY = ResArray[3];
      float RefZ = ResArray[4];
      float SerZ = ResArray[5];
      const bool PassX = CompareFloatEpsilon(SerX, RefX, 0.0008f);
      const bool PassY = CompareFloatEpsilon(SerY, RefY, 0.0008f);
      const bool PassZ = CompareFloatEpsilon(SerZ, RefZ, 0.0008f);
      if (!PassX || !PassY || !PassZ) {
        VERIFY_ARE_EQUAL(SerX, RefX);
        VERIFY_ARE_EQUAL(SerY, RefY);
        VERIFY_ARE_EQUAL(SerZ, RefZ);
        break;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetWorldRayDirection() PASSED");
  }

  // ObjectRayOrigin
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetObjectRayOrigin()");
    std::vector<int> TestData(WindowSize * WindowSize * 6, 0);
    LPCWSTR Args[] = {L"-HV 2021", L"-Vd", L"-DHIT_GET_VECTOR=ObjectRayOrigin",
                      L"-DMISS_GET_VECTOR=WorldRayOrigin",
                      L"-DSER_GET_VECTOR=GetObjectRayOrigin"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 3 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 6) {
      float *ResArray = (float *)(TestData.data() + Id);
      float RefX = ResArray[0];
      float SerX = ResArray[1];
      float RefY = ResArray[2];
      float SerY = ResArray[3];
      float RefZ = ResArray[4];
      float SerZ = ResArray[5];
      const bool PassX = CompareFloatEpsilon(SerX, RefX, 0.0008f);
      const bool PassY = CompareFloatEpsilon(SerY, RefY, 0.0008f);
      const bool PassZ = CompareFloatEpsilon(SerZ, RefZ, 0.0008f);
      if (!PassX || !PassY || !PassZ) {
        VERIFY_ARE_EQUAL(SerX, RefX);
        VERIFY_ARE_EQUAL(SerY, RefY);
        VERIFY_ARE_EQUAL(SerZ, RefZ);
        break;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetObjectRayOrigin() PASSED");
  }

  // ObjectRayDirection
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetObjectRayDirection()");
    std::vector<int> TestData(WindowSize * WindowSize * 6, 0);
    LPCWSTR Args[] = {L"-HV 2021", L"-Vd",
                      L"-DHIT_GET_VECTOR=ObjectRayDirection",
                      L"-DMISS_GET_VECTOR=WorldRayDirection",
                      L"-DSER_GET_VECTOR=GetObjectRayDirection"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 3 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 6) {
      float *ResArray = (float *)(TestData.data() + Id);
      float RefX = ResArray[0];
      float SerX = ResArray[1];
      float RefY = ResArray[2];
      float SerY = ResArray[3];
      float RefZ = ResArray[4];
      float SerZ = ResArray[5];
      const bool PassX = CompareFloatEpsilon(SerX, RefX, 0.0008f);
      const bool PassY = CompareFloatEpsilon(SerY, RefY, 0.0008f);
      const bool PassZ = CompareFloatEpsilon(SerZ, RefZ, 0.0008f);
      if (!PassX || !PassY || !PassZ) {
        VERIFY_ARE_EQUAL(SerX, RefX);
        VERIFY_ARE_EQUAL(SerY, RefY);
        VERIFY_ARE_EQUAL(SerZ, RefZ);
        break;
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetObjectRayDirection() PASSED");
  }
}

TEST_F(ExecutionTest, SERMatrixGetterTest) {
  // SER: Test basic function of HitObject getters.
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
    int rayFlags;
};

struct[raypayload] PerRayData
{
    float elems[ROWS*COLS] : read(caller) : write(miss,closesthit);
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
        testBuffer[id + 2 * (r * COLS + c)] = refPayload.elems[r*COLS + c];
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

matrix<float, ROWS, COLS> getMatIdentity() {
  matrix<float, ROWS, COLS> mat = 0;
  mat[0][0] = 1.f;
  mat[1][1] = 1.f;
  mat[2][2] = 1.f;
  return mat;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    matrix<float, ROWS, COLS> mat = MISS_GET_MATRIX();
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        payload.elems[r*COLS + c] = mat[r][c];
      }
    }
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    // UNUSED
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    matrix<float, ROWS, COLS> mat = HIT_GET_MATRIX();
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        payload.elems[r*COLS + c] = mat[r][c];
      }
    }
}
)";

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  const int WindowSize = 64;

  // WorldToObject3x4
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetWorldToObject3x4()");
    std::vector<int> TestData(WindowSize * WindowSize * 24, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DHIT_GET_MATRIX=WorldToObject3x4",
                      L"-DMISS_GET_MATRIX=getMatIdentity",
                      L"-DSER_GET_MATRIX=GetWorldToObject3x4",
                      L"-DROWS=3",
                      L"-DCOLS=4"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 12 /*payloadCount*/,
               2 /*attributeCount*/);
    const int ROWS = 3;
    const int COLS = 4;
    for (int Id = 0; Id < TestData.size(); Id += 24) {
      float *ResArray = (float *)(TestData.data() + Id);
      for (int RowIdx = 0; RowIdx < ROWS; RowIdx++) {
        for (int ColIdx = 0; ColIdx < COLS; ColIdx++) {
          int RefIdx = 2 * (RowIdx * COLS + ColIdx);
          float Ref = ResArray[RefIdx];
          float Ser = ResArray[1 + RefIdx];
          if (!CompareFloatEpsilon(Ser, Ref, 0.0008f)) {
            VERIFY_ARE_EQUAL(Ser, Ref);
          }
        }
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetWorldToObject3x4() PASSED");
  }

  // WorldToObject4x3
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetWorldToObject4x3()");
    const int ROWS = 4;
    const int COLS = 3;
    std::vector<int> TestData(WindowSize * WindowSize * 2 * ROWS * COLS, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DHIT_GET_MATRIX=WorldToObject4x3",
                      L"-DMISS_GET_MATRIX=getMatIdentity",
                      L"-DSER_GET_MATRIX=GetWorldToObject4x3",
                      L"-DROWS=4",
                      L"-DCOLS=3"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 12 /*payloadCount*/,
               2 /*attributeCount*/);
    for (int Id = 0; Id < TestData.size(); Id += 2 * ROWS * COLS) {
      float *ResArray = (float *)(TestData.data() + Id);
      for (int RowIdx = 0; RowIdx < ROWS; RowIdx++) {
        for (int ColIdx = 0; ColIdx < COLS; ColIdx++) {
          int RefIdx = 2 * (RowIdx * COLS + ColIdx);
          float Ref = ResArray[RefIdx];
          float Ser = ResArray[1 + RefIdx];
          if (!CompareFloatEpsilon(Ser, Ref, 0.0008f)) {
            VERIFY_ARE_EQUAL(Ser, Ref);
          }
        }
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetWorldToObject4x3() PASSED");
  }

  // ObjectToWorld3x4
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetObjectToWorld3x4()");
    std::vector<int> TestData(WindowSize * WindowSize * 24, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DHIT_GET_MATRIX=ObjectToWorld3x4",
                      L"-DMISS_GET_MATRIX=getMatIdentity",
                      L"-DSER_GET_MATRIX=GetObjectToWorld3x4",
                      L"-DROWS=3",
                      L"-DCOLS=4"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 12 /*payloadCount*/,
               2 /*attributeCount*/);
    const int ROWS = 3;
    const int COLS = 4;
    for (int Id = 0; Id < TestData.size(); Id += 24) {
      float *ResArray = (float *)(TestData.data() + Id);
      for (int RowIdx = 0; RowIdx < ROWS; RowIdx++) {
        for (int ColIdx = 0; ColIdx < COLS; ColIdx++) {
          int RefIdx = 2 * (RowIdx * COLS + ColIdx);
          float Ref = ResArray[RefIdx];
          float Ser = ResArray[1 + RefIdx];
          if (!CompareFloatEpsilon(Ser, Ref, 0.0008f)) {
            VERIFY_ARE_EQUAL(Ser, Ref);
          }
        }
      }
    }
    WEX::Logging::Log::Comment(L"HitObject::GetObjectToWorld3x4() PASSED");
  }

  // ObjectToWorld4x3
  {
    WEX::Logging::Log::Comment(L"Testing HitObject::GetObjectToWorld4x3()");
    std::vector<int> TestData(WindowSize * WindowSize * 24, 0);
    LPCWSTR Args[] = {L"-HV 2021",
                      L"-Vd",
                      L"-DHIT_GET_MATRIX=ObjectToWorld4x3",
                      L"-DMISS_GET_MATRIX=getMatIdentity",
                      L"-DSER_GET_MATRIX=GetObjectToWorld4x3",
                      L"-DROWS=4",
                      L"-DCOLS=3"};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
               WindowSize, WindowSize, true /*useMesh*/,
               false /*useProceduralGeometry*/, 12 /*payloadCount*/,
               2 /*attributeCount*/);
    const int ROWS = 4;
    const int COLS = 3;
    for (int Id = 0; Id < TestData.size(); Id += 24) {
      float *ResArray = (float *)(TestData.data() + Id);
      for (int RowIdx = 0; RowIdx < ROWS; RowIdx++) {
        for (int ColIdx = 0; ColIdx < COLS; ColIdx++) {
          int RefIdx = 2 * (RowIdx * COLS + ColIdx);
          float Ref = ResArray[RefIdx];
          float Ser = ResArray[1 + RefIdx];
          if (!CompareFloatEpsilon(Ser, Ref, 0.0008f)) {
            VERIFY_ARE_EQUAL(Ser, Ref);
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
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*useMesh*/,
             false /*useProceduralGeometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 2);
  VERIFY_ARE_EQUAL(Histo[2], 4030);
  VERIFY_ARE_EQUAL(Histo[5], 66);
}

TEST_F(ExecutionTest, SERShaderTableIndexTest) {
  // Test SER with HitObject::SetShaderTableIndex and
  // HitObject::GetShaderTableIndex
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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

struct CustomAttrs
{
    float dist;
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

    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, 0, 1, 0, ray, payload);
    dx::MaybeReorderThread(hitObject);

    // Invoke hit/miss for triangle
    dx::HitObject::Invoke( hitObject, payload );

    if (hitObject.IsHit())
    {
        // Transform to an 'aabb' hit.
        hitObject.SetShaderTableIndex( 1 );
    }

    // Invoke hit/miss for aabb
    dx::HitObject::Invoke( hitObject, payload );

    if (hitObject.IsHit())
    {
        // Poison the test data if GetShaderTableIndex does not match SetShaderTableIndex.
        if (hitObject.GetShaderTableIndex() != 1)
            payload.visited = 12345;
    }

    int id = launchIndex.x + launchIndex.y * launchDim.x;
    testBuffer[id] = payload.visited;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
  if ((payload.visited & 4U) == 0)
    payload.visited |= 4U; // First 'miss' invocation
  else
    payload.visited |= 8U; // Second 'miss' invocation
}

// Triangles
[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    AcceptHitAndEndSearch();
}

// Triangle closest hit
[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 1U;
}

// AABB closest hit
[shader("closesthit")]
void chAABB(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 2U;
}

// Procedural
[shader("intersection")]
void intersection()
{
   // UNUSED
}

[shader("anyhit")]
void ahAABB(inout PerRayData payload, in CustomAttrs attrs)
{
    // UNUSED
}

)";

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*mesh*/,
             true /*procedural geometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];

  VERIFY_ARE_EQUAL(Histo.size(), 2);
  VERIFY_ARE_EQUAL(
      Histo[3],
      66); // 'closesthit' invoked at index 0, then 'chAABB' invoked at index 1
  VERIFY_ARE_EQUAL(Histo[12], 4030); // Miss shader invoked twice
}

TEST_F(ExecutionTest, SERLoadLocalRootTableConstantTest) {
  // Test SER with HitObject::LoadLocalRootTableConstant
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
    int rayFlags;    
};

struct[raypayload] PerRayData
{
    uint res : read(caller) : write(miss,closesthit,caller);
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

struct LocalConstants
{
    int c0;
    int c1;
    int c2;
    int c3;
};

RWStructuredBuffer<int> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);
ConstantBuffer<LocalConstants> localConstants : register(b1);

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
    payload.res = 0;

    // SER Test
#if 1
    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    dx::MaybeReorderThread(hitObject);
    int c0 =  hitObject.LoadLocalRootTableConstant(0);
    int c1 =  hitObject.LoadLocalRootTableConstant(4);
    int c2 =  hitObject.LoadLocalRootTableConstant(8);
    int c3 =  hitObject.LoadLocalRootTableConstant(12);
    int res = c0 | c1 | c2 | c3;
#else
    TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    int res = payload.res;
#endif
    
    int id = launchIndex.x + launchIndex.y * launchDim.x;
    testBuffer[id] = res;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    payload.res = localConstants.c0 | localConstants.c1 | localConstants.c2 | localConstants.c3;
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    // UNUSED
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.res = localConstants.c0 | localConstants.c1 | localConstants.c2 | localConstants.c3;
}

)";

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*useMesh*/,
             false /*useProceduralGeometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 1);
  VERIFY_ARE_EQUAL(Histo[126], 4096);
}

TEST_F(ExecutionTest, SERRayQueryTest) {
  // Test SER RayQuery
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*useMesh*/,
             false /*useProceduralGeometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 2);
  VERIFY_ARE_EQUAL(Histo[0], 66);
  VERIFY_ARE_EQUAL(Histo[2], 4030);
}

TEST_F(ExecutionTest, SERIntersectionTest) {
  // Test SER with Intersection and procedural geometry
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, false /*mesh*/,
             true /*procedural geometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 1);
  VERIFY_ARE_EQUAL(Histo[5], 4096); // All rays hitting the procedural geometry
}

TEST_F(ExecutionTest, SERGetAttributesTest) {
  // Test SER with HitObject::GetAttributes
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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
        // Use 255 to keep outside the HitKind range [0,15] we passthru for hits.
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
    // Intersection with circle on a plane (base, n, radius)
    // hitPos is intersection point with plane (base, n)
    float3 base = {0.0f,0.0f,0.5f};
    float3 n = normalize(float3(0.0f,0.5f,0.5f));
    float radius = 500.f;
    // Plane hit
    float t = dot(n, base - ObjectRayOrigin()) / dot(n, ObjectRayDirection());
    if (t > RayTCurrent() || t < RayTMin())
        return;
    float3 hitPos = ObjectRayOrigin() + t * ObjectRayDirection();
    float3 relHitPos = hitPos - base;
    // Circle hit
    float hitDist = length(relHitPos);
    if (hitDist > radius)
      return;

    CustomAttrs attrs;
    attrs.dist = hitDist;

    // Generate wave-incoherent hitKind
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint hitKind = 1U;
    if (launchIndex.x >= 32)
        hitKind |= 2U;
    if (launchIndex.y >= 32)
        hitKind |= 4U;
    if ((launchIndex.x + launchIndex.y) % 2 == 0)
        hitKind |= 8U;

    ReportHit(t, hitKind, attrs);
}

)";

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, false /*mesh*/,
             true /*procedural geometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];

  VERIFY_ARE_EQUAL(Histo.size(), 10);
  VERIFY_ARE_EQUAL(Histo[0], 1587);
  VERIFY_ARE_EQUAL(Histo[1], 277);
  VERIFY_ARE_EQUAL(Histo[3], 256);
  VERIFY_ARE_EQUAL(Histo[5], 167);
  VERIFY_ARE_EQUAL(Histo[7], 153);
  VERIFY_ARE_EQUAL(Histo[9], 249);
  VERIFY_ARE_EQUAL(Histo[11], 260);
  VERIFY_ARE_EQUAL(Histo[13], 158);
  VERIFY_ARE_EQUAL(Histo[15], 142);
  VERIFY_ARE_EQUAL(Histo[255], 847);
}

TEST_F(ExecutionTest, SERTraceHitMissNopTest) {
  // Test SER with conditional HitObject::TraceRay, HitObject::IsHit,
  // HitObject::IsMiss, HitObject::IsNop
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*mesh*/,
             false /*procedural geometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 3);
  VERIFY_ARE_EQUAL(
      Histo[1],
      2048); // isNop && !isMiss && !isHit && !anyhit && !closesthit && !miss
  VERIFY_ARE_EQUAL(
      Histo[18],
      2015); // !isNop && isMiss && !isHit && !anyhit && !closesthit && miss
  VERIFY_ARE_EQUAL(
      Histo[44],
      33); // !isNop && !isMiss && isHit && anyhit && closesthit && !miss
}

TEST_F(ExecutionTest, SERIsMissTest) {
  // Test SER with HitObject::IsMiss
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*mesh*/,
             false /*procedural geometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 2);
  VERIFY_ARE_EQUAL(Histo[2], 4030);
  VERIFY_ARE_EQUAL(Histo[5], 66);
}

TEST_F(ExecutionTest, SERInvokeNoSBTTest) {
  // Test SER RayQuery with Invoke
  static const char *ShaderSrc = R"(
struct SceneConstants
{
    float4 eye;
    float4 U;
    float4 V;
    float4 W;
    float sceneScale;
    uint2 WindowSize;
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

    dx::HitObject hit = dx::HitObject::FromRayQuery(rayQ);
    dx::MaybeReorderThread(hit);
    // Set the payload based on the HitObject.
    if (hit.IsHit())
        payload.visited |= 8U;
    else
        payload.visited |= 16U;
    // Invoke should not trigger any shader.
    dx::HitObject::Invoke(hit, payload);

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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, false))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*useMesh*/,
             false /*useProceduralGeometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 2);
  VERIFY_ARE_EQUAL(Histo[8], 66);
  VERIFY_ARE_EQUAL(Histo[16], 4030);
}

TEST_F(ExecutionTest, SERMaybeReorderThreadTest) {
  // SER: Test MaybeReorderThread variants.
  static const char *ShaderSrc = R"(
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

    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

    if (launchIndex.x % 3 == 0) {
      dx::MaybeReorderThread(hitObject);
    }
    else if (launchIndex.x % 3 == 1) {
      dx::MaybeReorderThread(hitObject, 0xFF, 7);
    }
    else {
      dx::MaybeReorderThread(0xFFF, 5);
    } 

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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, false))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*useMesh*/,
             false /*useProceduralGeometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 2);
  VERIFY_ARE_EQUAL(Histo[2], 4030);
  VERIFY_ARE_EQUAL(Histo[5], 66);
}

TEST_F(ExecutionTest, SERDynamicHitObjectArrayTest) {
  // Test SER with dynamic access to local HitObject array
  static const char *ShaderSrc = R"(
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
    uint dummy : read(caller) : write(miss, closesthit);
};

struct LocalConstants
{
    int c0;
    int c1;
    int c2;
    int c3;
};

RWStructuredBuffer<int> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);
ConstantBuffer<LocalConstants> localConstants : register(b1);

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

    int constants[4] = { localConstants.c0, localConstants.c1, localConstants.c2, localConstants.c3 };
    
    const int NUM_SAMPLES = 64;
    const int NUM_HITOBJECTS = 8;

    // Generate wave-incoerent sample positions
    int sampleIndices[NUM_SAMPLES];
    int threadOffset = launchIndex.x;
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        int baseIndex = i % 4; // Cycle through the 4 constants
        sampleIndices[i] = abs(constants[baseIndex] + threadOffset + i * 3) % NUM_HITOBJECTS;
    }

    // Define an array of ray flags
    uint rayFlagsArray[NUM_HITOBJECTS] = {
        RAY_FLAG_NONE,
        RAY_FLAG_FORCE_OPAQUE,
        RAY_FLAG_FORCE_NON_OPAQUE,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        RAY_FLAG_CULL_FRONT_FACING_TRIANGLES,
        RAY_FLAG_CULL_OPAQUE
    };

    // Create a local array of HitObjects with TraceRay
    dx::HitObject hitObjects[NUM_HITOBJECTS];
    for (uint i = 0; i < NUM_HITOBJECTS; ++i)
    {
        PerRayData payload;
        uint expectedRayFlags = rayFlagsArray[i];
        hitObjects[i] = dx::HitObject::TraceRay(
            topObject,          // Acceleration structure
            expectedRayFlags,   // Unique ray flag
            0xFF,               // Instance mask
            0,                  // Ray contribution to hit group index
            1,                  // Multiplier for geometry contribution
            0,                  // Miss shader index
            ray,                // Ray description
            payload             // Payload
        );
    }

    // Evaluate at sample positions.
    int testVal = 0;

    for (uint i = 0; i < NUM_SAMPLES; i++)
    {
        int idx = sampleIndices[i];
        // Verify that the rayFlags match
        uint actualRayFlags = hitObjects[idx].GetRayFlags();
        uint expectedRayFlags = rayFlagsArray[idx];
        if (expectedRayFlags != actualRayFlags)
        {
            testVal = 1; // Mark as failure if flags do not match
        }
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
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    AcceptHitAndEndSearch();
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    // UNUSED
}

)";

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, false))
    return;

  // Initialize test data.
  const int WindowSize = 64;

  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};
  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*mesh*/,
             false /*procedural geometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 1);
  VERIFY_ARE_EQUAL(Histo[0], 4096);
}

TEST_F(ExecutionTest, SERWaveIncoherentHitTest) {
  // Test SER with wave incoherent conditional assignment of HitObject values
  //    with and without procedural attributes.
  static const char *ShaderSrc = R"(
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

struct CustomAttrs
{
    float dist;
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

static const uint ProceduralHitKind = 11;

[shader("raygeneration")]
void raygen()
{
    uint2   launchIndex = DispatchRaysIndex().xy;
    uint2   launchDim = DispatchRaysDimensions().xy;

    RayDesc ray = ComputeRay();

    PerRayData payload;
    payload.visited = 0;

    dx::HitObject hitObject;

    // Use wave incoherence to decide how to create the HitObject
    if (launchIndex.x % 4 == 1)
    {
        ray.Origin.x += 2.0f;
        hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 0, 0, ray, payload);
    }
    else if (launchIndex.x % 4 == 2)
    {
        hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, 0, 0, 0, ray, payload);
    }
    else if (launchIndex.x % 4 == 3)
    {
        hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_TRIANGLES, 0xFF, 0, 0, 0, ray, payload);
    }

    dx::MaybeReorderThread(hitObject);

    if (hitObject.IsNop())
        payload.visited |= 1U;
    if (hitObject.IsMiss())
        payload.visited |= 2U;

    if (hitObject.GetHitKind() == ProceduralHitKind)
        payload.visited |= 8U;
    else if (hitObject.IsHit())
        payload.visited |= 4U;

    dx::HitObject::Invoke(hitObject, payload);

    // Store the result in the buffer
    int id = launchIndex.x + launchIndex.y * launchDim.x;
    testBuffer[id] = payload.visited;
}

[shader("miss")]
void miss(inout PerRayData payload)
{
    // UNUSED
}

// Triangles
[shader("anyhit")]
void anyhit(inout PerRayData payload, in Attrs attrs)
{
    AcceptHitAndEndSearch();
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in Attrs attrs)
{
    payload.visited |= 16U;
}

// Procedural
[shader("closesthit")]
void chAABB(inout PerRayData payload, in CustomAttrs attrs)
{
    payload.visited |= 32U;
}

[shader("anyhit")]
void ahAABB(inout PerRayData payload, in CustomAttrs attrs)
{
    // UNUSED
}

[shader("intersection")]
void intersection()
{
    // Intersection with circle on a plane (base, n, radius)
    // hitPos is intersection point with plane (base, n)
    float3 base = {0.0f,0.0f,0.5f};
    float3 n = normalize(float3(0.0f,0.5f,0.5f));
    float radius = 1000.f;
    // Plane hit
    float t = dot(n, base - ObjectRayOrigin()) / dot(n, ObjectRayDirection());
    if (t > RayTCurrent() || t < RayTMin()) {
        return;
    }
    float3 hitPos = ObjectRayOrigin() + t * ObjectRayDirection();
    float3 relHitPos = hitPos - base;
    // Circle hit
    float hitDist = length(relHitPos);
    if (hitDist > radius)
      return;

    CustomAttrs attrs;
    attrs.dist = hitDist;
    ReportHit(t, ProceduralHitKind, attrs);
}

)";

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, false))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*mesh*/,
             true /*procedural geometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 6);
  VERIFY_ARE_EQUAL(Histo[1], 1024);  // nop
  VERIFY_ARE_EQUAL(Histo[2], 1022);  // miss
  VERIFY_ARE_EQUAL(Histo[4], 12);    // triangle hit, no ch
  VERIFY_ARE_EQUAL(Histo[8], 1008);  // procedural hit, no ch
  VERIFY_ARE_EQUAL(Histo[20], 11);   // triangle hit, 'closesthit' invoked
  VERIFY_ARE_EQUAL(Histo[40], 1019); // procedural hit, 'chAABB' invoked
}

TEST_F(ExecutionTest, SERReorderCoherentTest) {
  // SER: Test reordercoherent
  static const char *ShaderSrc = R"(
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

reordercoherent RWStructuredBuffer<int> testBuffer : register(u0);
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
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    uint threadId = launchIndex.x + launchIndex.y * launchDim.x;

    RayDesc ray = ComputeRay();

    PerRayData payload;
    payload.visited = 0;

    // Initial test value.
    testBuffer[threadId] = threadId;

    dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

    // Conditionally update the test value.
    if (hitObject.IsHit())
    {
        testBuffer[threadId] += 10; // Add 10 to hits
    }
    else
    {
        testBuffer[threadId] += 20; // Add 20 to misses
    }

    Barrier(UAV_MEMORY, REORDER_SCOPE);
    dx::MaybeReorderThread(hitObject);

    // Conditionally update the test value.
    if (threadId % 2 == 0)
    {
        testBuffer[threadId] += 1000; // Add 1000 to even threads
    }
    else
    {
        testBuffer[threadId] += 2000; // Add 2000 to odd threads
    }

    // Verify test value.
    uint expectedValue = (hitObject.IsHit() ? threadId + 10 : threadId + 20);
    expectedValue += (threadId % 2 == 0 ? 1000 : 2000);
    if (testBuffer[threadId] != expectedValue)
    {
        // Mark failure in the buffer if the result does not match
        testBuffer[threadId] = 0;
    }
    else
    {
        testBuffer[threadId] = 1;
    }
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

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, false))
    return;

  // Initialize test data.
  const int WindowSize = 64;
  std::vector<int> TestData(WindowSize * WindowSize, 0);
  LPCWSTR Args[] = {L"-HV 2021", L"-Vd"};

  RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, _countof(Args), TestData,
             WindowSize, WindowSize, true /*useMesh*/,
             false /*useProceduralGeometry*/, 1 /*payloadCount*/,
             2 /*attributeCount*/);
  std::map<int, int> Histo;
  for (int Val : TestData)
    ++Histo[Val];
  VERIFY_ARE_EQUAL(Histo.size(), 1);
  VERIFY_ARE_EQUAL(Histo[1], 4096);
}