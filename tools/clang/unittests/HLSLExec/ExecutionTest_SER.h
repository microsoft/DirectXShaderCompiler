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

struct SERAccessor {
  enum ScalarTypes {
    UINT = 0,
    FLOAT = 1,
  };

  ScalarTypes ScalarType;
  int ValRows;
  int ValCols;

  LPCWSTR HitObjectGetter;
  LPCWSTR CHGetter;
  LPCWSTR MSGetter;
  LPCWSTR NOPGetter;

  LPCWSTR getScalarTypeName() const {
    switch (ScalarType) {
    case UINT:
      return L"uint";
    case FLOAT:
      return L"float";
    default:
      return L"UNKOWN TYPE";
    }
  }

  void addCompileArgs(std::vector<std::wstring> &OwnedArgs,
                      std::vector<LPCWSTR> &ArgVec) const {
    // Value dimensions
    OwnedArgs.emplace_back(L"-DM_ROWS=" + std::to_wstring(ValRows));
    ArgVec.push_back(OwnedArgs.back().c_str());
    OwnedArgs.emplace_back(L"-DM_COLS=" + std::to_wstring(ValCols));
    ArgVec.push_back(OwnedArgs.back().c_str());

    LPCWSTR ScalarTypeName = getScalarTypeName();
    OwnedArgs.emplace_back(L"-DSCALAR_TYPE=" + std::wstring(ScalarTypeName));
    ArgVec.push_back(OwnedArgs.back().c_str());
    if (ValRows == 0 && ValCols == 0) {
      // Scalar
      OwnedArgs.emplace_back(L"-DRESULT_TYPE=" + std::wstring(ScalarTypeName));
      ArgVec.push_back(OwnedArgs.back().c_str());
    } else if (ValRows >= 1 && ValCols == 0) {
      // Vector
      OwnedArgs.emplace_back(L"-DRESULT_TYPE=" + std::wstring(ScalarTypeName) +
                             std::to_wstring(ValRows));
      ArgVec.push_back(OwnedArgs.back().c_str());
    } else if (ValRows > 0 && ValCols > 0) {
      // Matrix
      OwnedArgs.emplace_back(L"-DMATRIX_ELEMENT_TYPE=" +
                             std::wstring(ScalarTypeName));
      ArgVec.push_back(OwnedArgs.back().c_str());
    }

    OwnedArgs.emplace_back(L"-DHITOBJECT_GET_RESULT=" +
                           std::wstring(HitObjectGetter));
    ArgVec.push_back(OwnedArgs.back().c_str());
    OwnedArgs.emplace_back(L"-DCH_GET_RESULT=" + std::wstring(CHGetter));
    ArgVec.push_back(OwnedArgs.back().c_str());
    OwnedArgs.emplace_back(L"-DMS_GET_RESULT=" + std::wstring(MSGetter));
    ArgVec.push_back(OwnedArgs.back().c_str());
    OwnedArgs.emplace_back(L"-DNOP_GET_RESULT=" + std::wstring(NOPGetter));
    ArgVec.push_back(OwnedArgs.back().c_str());
  }
};

struct SERTestConfig {
  // Source of the hit object or reference value under test.
  enum Method {
    TraceRay =
        0, // Source queried in closesthit, miss shaders called by TraceRay
    RayQuery = 1,           // Source is HitObject::FromRayQuery
    HitObject_TraceRay = 2, // Source is HitObject::TraceRay
    HitObject_Invoke = 3,   // [only used for recursion]
  };
  std::wstring getMethodStr(Method src) const {
    switch (src) {
    case TraceRay:
      return L"TraceRay";
    case RayQuery:
      return L"RayQuery";
    case HitObject_TraceRay:
      return L"HitObject_TraceRay";
    default:
      return L"UNKNOWN";
    }
  }

  enum ResultFrom {
    FromShaders = 0,   // Call getters in CH, MS
    FromHitObject = 1, // Call getters on HitObject
  };
  std::wstring getResultFromStr(ResultFrom resultFrom) const {
    switch (resultFrom) {
    case FromShaders:
      return L"FromShaders";
    case FromHitObject:
      return L"FromHitObject";
    default:
      return L"UNKNOWN";
    }
  }

  // Where the hit object code is located.
  enum TestLocation {
    RayGen = 0,     // In raygen shader
    ClosestHit = 1, // In closesthit shader
    Miss = 2,       // In miss shader
  };
  std::wstring getTestLocationStr(TestLocation loc) const {
    switch (loc) {
    case RayGen:
      return L"RayGen";
    case ClosestHit:
      return L"ClosestHit";
    case Miss:
      return L"Miss";
    default:
      return L"UNKNOWN";
    }
  }

  bool UseTriangles;
  bool UseProceduralGeometry;

  bool ReorderHitObject;
  TestLocation TestLoc;

  Method TraceMethod;
  ResultFrom ResultSrc;

  Method RecMethod; // only used if TestLoc != RayGen

  // TestLocation TestLocation;
  //
  const bool hasRecursion() const { return TestLoc != TestLocation::RayGen; }

  void addCompileArgs(std::vector<LPCWSTR> &ArgVec) const {
    // How to produce the hit object and get the value from it
    switch (TraceMethod) {
    case TraceRay:
      // Getter called on HitObject produced by HitObject::TraceRay
      ArgVec.push_back(L"-DMETHOD_TRACERAY=1");
      break;
    case HitObject_TraceRay:
      // Getter called on HitObject produced by HitObject::TraceRay
      ArgVec.push_back(L"-DMETHOD_HITOBJECT_TRACERAY=1");
      break;
    case RayQuery:
      // Getter called on HitObject produced by HitObject::FromRayQuery
      ArgVec.push_back(L"-DMETHOD_HITOBJECT_FROMRQ=1");
      break;
    default:
      VERIFY_IS_TRUE(false);
      break;
    }

    switch (ResultSrc) {
    case FromShaders:
      ArgVec.push_back(L"-DRESULT_FROM_SHADERS=1");
      break;
    case FromHitObject:
      ArgVec.push_back(L"-DRESULT_FROM_HITOBJECT=1");
      break;
    default:
      VERIFY_IS_TRUE(false);
      break;
    }

    if (ReorderHitObject)
      ArgVec.push_back(L"-DREORDER_HITOBJECT=1");

    switch (TestLoc) {
    case TestLocation::RayGen:
      ArgVec.push_back(L"-DTESTLOC_RAYGEN=1");
      break;
    case TestLocation::ClosestHit:
      ArgVec.push_back(L"-DTESTLOC_CLOSESTHIT=1");
    case TestLocation::Miss:
      ArgVec.push_back(L"-DTESTLOC_MISS=1");
      break;
    default:
      VERIFY_IS_TRUE(false);
      break;
    }

    if (hasRecursion()) {
      ArgVec.push_back(L"-DENABLE_RECURSION=1");

      // Primary shading call to test HitObject in CH/MS
      switch (RecMethod) {
      case TraceRay:
        ArgVec.push_back(L"-DRECMETHOD_TRACERAY=1");
        break;
      case HitObject_Invoke:
        ArgVec.push_back(L"-DRECMETHOD_HITOBJECT_INVOKE=1");
        break;
      default:
        VERIFY_IS_TRUE(false);
        break;
      }
    }
  }

  std::wstring str() const {
    std::wstring txt;
    if (UseTriangles)
      txt += L"tris;";
    if (UseProceduralGeometry)
      txt += L"aabbs;";
    txt += L"trace=" + getMethodStr(TraceMethod) + L";";
    txt += L"result=" + getResultFromStr(ResultSrc) + L";";
    txt += L"loc=" + getTestLocationStr(TestLoc) + L";";
    if (ReorderHitObject) {
      txt += L"reorder;";
    }
    if (hasRecursion()) {
      txt += L"rec;";
    }
    return txt;
  }
};

// clang-format off
static constexpr SERAccessor Accessors[] = {
  // Scalar
  {SERAccessor::FLOAT, 0, 0, L"GetRayTMin", L"RayTMin", L"RayTMin", L"getFloatZero"},
  {SERAccessor::FLOAT, 0, 0, L"GetRayTCurrent", L"RayTCurrent", L"RayTCurrent", L"getFloatZero"},
  {SERAccessor::UINT,  0, 0, L"GetRayFlags", L"RayFlags", L"RayFlags", L"getIntZero"},
  {SERAccessor::UINT,  0, 0, L"GetHitKind", L"HitKind", L"getIntZero", L"getIntZero"},
  {SERAccessor::UINT,  0, 0, L"GetGeometryIndex", L"GeometryIndex", L"getIntZero", L"getIntZero"},
  {SERAccessor::UINT,  0, 0, L"GetInstanceIndex", L"InstanceIndex", L"getIntZero", L"getIntZero"},
  {SERAccessor::UINT,  0, 0, L"GetInstanceID", L"InstanceID", L"getIntZero", L"getIntZero"},
  {SERAccessor::UINT,  0, 0, L"GetPrimitiveIndex", L"PrimitiveIndex", L"getIntZero", L"getIntZero"},
  {SERAccessor::UINT,  0, 0, L"IsHit", L"getIntOne", L"getIntZero", L"getIntZero"},
  {SERAccessor::UINT,  0, 0, L"IsNop", L"getIntZero", L"getIntZero", L"getIntOne"},
  {SERAccessor::UINT,  0, 0, L"IsMiss", L"getIntZero", L"getIntOne", L"getIntZero"},
  // Vector
  {SERAccessor::FLOAT, 3, 0, L"GetWorldRayOrigin", L"WorldRayOrigin", L"WorldRayOrigin", L"getVec3Zero"},
  {SERAccessor::FLOAT, 3, 0, L"GetWorldRayDirection", L"WorldRayDirection", L"WorldRayDirection", L"getVec3Zero"},
  {SERAccessor::FLOAT, 3, 0, L"GetObjectRayOrigin", L"ObjectRayOrigin", L"WorldRayOrigin", L"getVec3Zero"},
  {SERAccessor::FLOAT, 3, 0, L"GetObjectRayDirection", L"ObjectRayDirection", L"WorldRayDirection", L"getVec3Zero"},
  // Matrix
  {SERAccessor::FLOAT, 3, 4, L"GetWorldToObject3x4", L"WorldToObject3x4", L"getOneDiagonalMat", L"getOneDiagonalMat"},
  {SERAccessor::FLOAT, 4, 3, L"GetWorldToObject4x3", L"WorldToObject4x3", L"getOneDiagonalMat", L"getOneDiagonalMat"},
  {SERAccessor::FLOAT, 3, 4, L"GetObjectToWorld3x4", L"ObjectToWorld3x4", L"getOneDiagonalMat", L"getOneDiagonalMat"},
  {SERAccessor::FLOAT, 4, 3, L"GetObjectToWorld4x3", L"ObjectToWorld4x3", L"getOneDiagonalMat", L"getOneDiagonalMat"},
};
// clang-format on

static const char *SERPermutationTestShaderSrc = R"(

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

#ifdef MATRIX_ELEMENT_TYPE
typedef matrix<MATRIX_ELEMENT_TYPE, M_ROWS, M_COLS> ValueType;
#else
typedef RESULT_TYPE ValueType;
#endif

struct [raypayload] PerRayData
{
    int recursionDepth : read(caller,closesthit,miss) : write(caller,closesthit,miss);
};

struct TriangleAttrs
{
    float2 barycentrics;
};

RWStructuredBuffer<SCALAR_TYPE> testBuffer : register(u0);
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

#ifdef MATRIX_ELEMENT_TYPE
typedef matrix<MATRIX_ELEMENT_TYPE, M_ROWS, M_COLS> MatrixType;

matrix<float, M_ROWS, M_COLS> getOneDiagonalMat() {
  matrix<float, M_ROWS, M_COLS> mat = 0;
  mat[0][0] = 1.f;
  mat[1][1] = 1.f;
  mat[2][2] = 1.f;
  return mat;
}
#endif

void StoreResult(ValueType result) {
  const int numRows = M_ROWS > 0 ? M_ROWS : 1;
  const int numCols = M_COLS > 0 ? M_COLS : 1;
  const int numResultElements = numRows * numCols;

  uint2 launchIndex = DispatchRaysIndex().xy;
  uint2 launchDim = DispatchRaysDimensions().xy;
  const int id = numResultElements * (launchIndex.x + launchIndex.y * launchDim.x);

#ifdef MATRIX_ELEMENT_TYPE
#if M_ROWS == 0 || M_COLS == 0
#error "Zero-sized matrix dimension"
#endif

  // Matrix
  for (int r = 0; r < M_ROWS; r++) {
    for (int c = 0; c < M_COLS; c++) {
      testBuffer[id + (r * M_COLS + c)] = result[r][c];
    }
  }

#elif M_ROWS
#if M_COLS
#error "Rows specified for vector"
#endif
  // Vector
  for (int r = 0; r < M_ROWS; r++) {
    testBuffer[id + r] = result[r];
  }
#else
  testBuffer[id] = result;
#endif
}

// Procedural geometry for use by RayQuery and intersection shader
static const int ProceduralHitKind = 11;

struct CustomAttrs
{
  float dist;
};

bool evalIntersection(float3 objRayOrigin, float3 objRayDir, float rayTMax, float rayTMin, out CustomAttrs attrs, out float rayT)
{
  rayT = 0;
  // Intersection with circle on a plane (base, n, radius)
  // hitPos is intersection point with plane (base, n)
  float3 base = {0.0f,0.0f,0.5f};
  float3 n = normalize(float3(0.0f,0.5f,0.5f));
  float radius = 500.f;
  // Plane hit
  float t = dot(n, base - objRayOrigin) / dot(n, objRayDir);
  if (t > rayTMax || t < rayTMin) {
      return false;
  }
  float3 hitPos = objRayOrigin + t * objRayDir;
  float3 relHitPos = hitPos - base;
  // Circle hit
  float hitDist = length(relHitPos);
  if (hitDist > radius)
    return false;

  attrs.dist = hitDist;
  rayT = t;
  return true;
}

#if ATTRIBUTES_TEST
void StoreTriangleAttributes(TriangleAttrs attrs) {
  float2 resValue = attrs.barycentrics;
  StoreResult(resValue);
}

void StoreProceduralAttributes(CustomAttrs attrs) {
  float2 resValue = {attrs.dist, 0};
  StoreResult(resValue);
}
#endif


static dx::HitObject hitObjectTraceFromRQ(RayDesc ray) {
  RayQuery<RAY_FLAG_NONE> rayQ;
  rayQ.TraceRayInline(topObject, RAY_FLAG_NONE, 0xFF, ray);

  float tHit = 0;
  CustomAttrs customAttrs = {0};

  while (rayQ.Proceed()) {
    switch (rayQ.CandidateType()) {
    
    // Acccept all triangle hits
    case CANDIDATE_NON_OPAQUE_TRIANGLE: {
      rayQ.CommitNonOpaqueTriangleHit();
      break;
    }

    // Use same decision logic as intersection shader
    case CANDIDATE_PROCEDURAL_PRIMITIVE: {
      if (evalIntersection(rayQ.CandidateObjectRayOrigin(), rayQ.CandidateObjectRayDirection(), rayQ.CommittedRayT(), rayQ.RayTMin(), customAttrs, tHit)) {
        rayQ.CommitProceduralPrimitiveHit(tHit);
      }
      break;
    }

    default:
       break;
    }
  }

  switch (rayQ.CommittedStatus()) {
  case COMMITTED_NOTHING:
    return dx::HitObject::MakeMiss(RAY_FLAG_NONE, 0, ray);
  case COMMITTED_TRIANGLE_HIT: {
    TriangleAttrs attrs;
    attrs.barycentrics = rayQ.CommittedTriangleBarycentrics();
    uint HitKind = rayQ.CommittedTriangleFrontFace() ? HIT_KIND_TRIANGLE_FRONT_FACE : HIT_KIND_TRIANGLE_BACK_FACE;
    dx::HitObject hitObject = dx::HitObject::FromRayQuery(rayQ, HitKind, attrs);
    hitObject.SetShaderTableIndex(0);
    return hitObject;
  }
  case COMMITTED_PROCEDURAL_PRIMITIVE_HIT: {
    dx::HitObject hitObject = dx::HitObject::FromRayQuery(rayQ, ProceduralHitKind, customAttrs);
    hitObject.SetShaderTableIndex(0);
    return hitObject;
  }
  default:
    return dx::HitObject();
  }
}

void CallTraceMethod(int recursionDepth) {
  const int numRows = M_ROWS > 0 ? M_ROWS : 1;
  const int numCols = M_COLS > 0 ? M_COLS : 1;
  const int numResultElements = numRows * numCols;

  uint2 launchIndex = DispatchRaysIndex().xy;
  uint2 launchDim = DispatchRaysDimensions().xy;
  const int id = numResultElements * (launchIndex.x + launchIndex.y * launchDim.x);

  RayDesc ray = ComputeRay();

  PerRayData payload;
#ifdef ENABLE_RECURSION
  payload.recursionDepth = recursionDepth;
#endif


#if METHOD_TRACERAY
///// Reference result
  TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
#if !RESULT_FROM_SHADERS
  #error "TraceRay() implicitly gets results from shaders"
#endif

///// Produce hit object
#elif METHOD_HITOBJECT_TRACERAY
  dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

#elif METHOD_HITOBJECT_FROMRQ
  dx::HitObject hitObject = hitObjectTraceFromRQ(ray);
#endif

#if REORDER_HITOBJECT
  dx::MaybeReorderThread(hitObject);
#endif

///// Query hit object getter directly
#if RESULT_FROM_HITOBJECT
#if ATTRIBUTES_TEST
  if (hitObject.IsMiss()) {
    // Test for zero-init of miss
    TriangleAttrs attrs;
#if NEW_GETATTRIBUTES_API
    hitObject.GetAttributes<TriangleAttrs>(attrs);
#else
    attrs = hitObject.GetAttributes<TriangleAttrs>();
#endif
    StoreTriangleAttributes(attrs);
  } else if (hitObject.GetHitKind() == ProceduralHitKind) {
    CustomAttrs attrs;
#if NEW_GETATTRIBUTES_API
    hitObject.GetAttributes<CustomAttrs>(attrs);
#else
    attrs = hitObject.GetAttributes<CustomAttrs>();
#endif
    StoreProceduralAttributes(attrs);
  } else {
    TriangleAttrs attrs;
#if NEW_GETATTRIBUTES_API
    hitObject.GetAttributes<TriangleAttrs>(attrs);
#else
    attrs = hitObject.GetAttributes<TriangleAttrs>();
#endif
    StoreTriangleAttributes(attrs);
  }
#else
  StoreResult(hitObject.HITOBJECT_GET_RESULT());
#endif

#elif RESULT_FROM_SHADERS
#if !METHOD_TRACERAY
  // Already invoked in TraceRay()
  dx::HitObject::Invoke(hitObject, payload);
#endif
#endif
}

[shader("raygeneration")]
void raygen()
{
#if ENABLE_RECURSION
  RayDesc ray = ComputeRay();
  PerRayData recPayload;
  recPayload.recursionDepth = 1;
#if RECMETHOD_TRACERAY
  TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, recPayload);
#elif RECMETHOD_HITOBJECT_INVOKE
  dx::HitObject hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, recPayload);
  dx::HitObject::Invoke(hitObject, recPayload);
#else
#error "Unsupported shading method in recursive tests"
#endif
  return;
#endif

#if TESTLOC_RAYGEN
  CallTraceMethod(1);
  return;
#if ENABLE_RECURSION
#error "Must disable recursion when testing in raygen"
#endif
#endif
}

float getFloatZero() { return 0.0f; }
int getIntZero() { return 0; }
int getIntOne() { return 1; }

[shader("miss")]
void miss(inout PerRayData payload)
{
#if TESTLOC_MISS
  if (payload.recursionDepth == 1)
  {
    CallTraceMethod(payload.recursionDepth + 1);
    return;
  }
#endif

#if ATTRIBUTES_TEST
  StoreResult(float2(0,0));
#else
  StoreResult(MS_GET_RESULT());
#endif
}

///// Triangle hit group
[shader("anyhit")]
void anyhit(inout PerRayData payload, in TriangleAttrs attrs)
{
  // UNUSED
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in TriangleAttrs attrs)
{
#if TESTLOC_CLOSESTHIT
  if (payload.recursionDepth == 1)
  {
    CallTraceMethod(payload.recursionDepth + 1);
    return;
  }
#endif

#if ATTRIBUTES_TEST
  StoreTriangleAttributes(attrs);
#else
  StoreResult(CH_GET_RESULT());
#endif
}

///// AABB hit group
[shader("closesthit")]
void chAABB(inout PerRayData payload, in CustomAttrs customAttrs)
{
#if TESTLOC_CLOSESTHIT
  if (payload.recursionDepth == 1)
  {
    CallTraceMethod(payload.recursionDepth + 1);
    return;
  }
#endif

#if ATTRIBUTES_TEST
  StoreProceduralAttributes(customAttrs);
#else
  StoreResult(CH_GET_RESULT());
#endif
}

[shader("intersection")]
void intersection()
{
  CustomAttrs attrs = {0};
  float rayT;
  if (evalIntersection(ObjectRayOrigin(), ObjectRayDirection(), RayTCurrent(), RayTMin(), attrs, rayT)) {
    ReportHit(rayT, ProceduralHitKind, attrs);
  }
}

[shader("anyhit")]
void ahAABB(inout PerRayData payload, in CustomAttrs attrs)
{
  // UNUSED
}

)";

template<typename T>
static void VerifyTestArray(const T* RefData, const T* TestData, int NumElements);

template<>
void VerifyTestArray(const int* RefData, const int* TestData, int NumElements) {
  for (int i = 0; i < NumElements; i++) {
    if (RefData[i] != TestData[i]) {
      VERIFY_ARE_EQUAL(RefData[i], TestData[i]);
    }
  }
}

template<>
void VerifyTestArray(const float* RefData, const float* TestData, int NumElements) {
  for (int i = 0; i < NumElements; i++) {
    const float RefVal = RefData[i];
    const float TestVal = TestData[i];
    if (!CompareFloatEpsilon(TestVal, RefVal, 0.0008f)) {
      VERIFY_ARE_EQUAL(TestVal, RefVal);
    }
  }
}

TEST_F(ExecutionTest, SERGetterPermutationTest) {
  // SER: Test basic function of HitObject getters.

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  SERTestConfig RefConfig = {true,
                             true,
                             false,
                             SERTestConfig::RayGen,
                             SERTestConfig::TraceRay,
                             SERTestConfig::FromShaders,
                             SERTestConfig::TraceRay};

  std::vector<SERTestConfig> TestConfigs;
  for (SERTestConfig::TestLocation TestLoc :
       {SERTestConfig::RayGen, SERTestConfig::Miss,
        SERTestConfig::ClosestHit}) {
    for (bool Reorder : {true, false}) {
      // MaybeReorderThreads only supported in RayGens
      if (TestLoc != SERTestConfig::RayGen && Reorder)
        continue;

      for (SERTestConfig::Method TestMethod :
           {SERTestConfig::HitObject_TraceRay, SERTestConfig::RayQuery}) {
        for (SERTestConfig::ResultFrom ResultSrc :
             {SERTestConfig::FromShaders, SERTestConfig::FromHitObject}) {
          SERTestConfig TestConfig = RefConfig;
          TestConfig.TestLoc = TestLoc;
          TestConfig.TraceMethod = TestMethod;
          TestConfig.ReorderHitObject = Reorder;
          TestConfig.ResultSrc = ResultSrc;

          if (TestLoc == SERTestConfig::RayGen) {
            TestConfigs.push_back(TestConfig);
            continue;
          }

          // Variations on primary shading call to test HitObject in CH/MS
          for (SERTestConfig::Method RecMethod :
               {SERTestConfig::TraceRay, SERTestConfig::HitObject_Invoke}) {
            TestConfig.RecMethod = RecMethod;
            TestConfigs.push_back(TestConfig);
          }
        }
      }
    }
  }

  // 64 x 64 test window size
  const int WindowSize = 64;

  for (const auto &Accessor : Accessors) {
    const int NumResultRows = Accessor.ValRows > 0 ? Accessor.ValRows : 1;
    const int NumResultCols = Accessor.ValCols > 0 ? Accessor.ValCols : 1;
    const int NumResultElements = NumResultRows * NumResultCols;
    const int RefMaxRecursion = RefConfig.hasRecursion() ? 2 : 1;

    // Query reference result
    std::vector<int> RefData(WindowSize * WindowSize * NumResultElements);
    std::vector<LPCWSTR> RefArgs;
    std::vector<std::wstring> OwnedRefArgs;
    RefArgs.push_back(L"-HV 2021");
    RefArgs.push_back(L"-Vd");
    Accessor.addCompileArgs(OwnedRefArgs, RefArgs);
    RefConfig.addCompileArgs(RefArgs);

    const int ExtraRec = 0;
    DXRRunConfig RefRunConfig = {
        WindowSize,
        WindowSize,
        RefConfig.UseTriangles,
        RefConfig.UseProceduralGeometry,
        RefMaxRecursion + ExtraRec,
    };
    RunDXRTest(Device, SERPermutationTestShaderSrc, L"lib_6_9", RefArgs.data(),
               (int)RefArgs.size(), RefData, RefRunConfig);

    // Test permutations
    for (const auto &TestConfig : TestConfigs) {
      DXRRunConfig TestRunConfig(RefRunConfig);
      TestRunConfig.MaxRecursion =
          ExtraRec + (TestConfig.hasRecursion() ? 2 : 1);

      std::wstring TestConfigTxt = L"HitObject::";
      TestConfigTxt += Accessor.HitObjectGetter;
      TestConfigTxt += L"() with config " + TestConfig.str();

      {
        std::wstring TestingMsg = L"Testing " + TestConfigTxt;
        WEX::Logging::Log::Comment(TestingMsg.c_str());
      }

      std::vector<LPCWSTR> Args;
      std::vector<std::wstring> OwnedArgs;
      Args.push_back(L"-HV 2021");
      Args.push_back(L"-Vd");
      Accessor.addCompileArgs(OwnedArgs, Args);
      TestConfig.addCompileArgs(Args);

      std::vector<int> TestData(WindowSize * WindowSize * NumResultElements, 0);

      RunDXRTest(Device, SERPermutationTestShaderSrc, L"lib_6_9", Args.data(),
                 (int)Args.size(), TestData, TestRunConfig);

      const int NumArrayElems = WindowSize * WindowSize * NumResultElements;
      switch (Accessor.ScalarType) {
      case SERAccessor::FLOAT:
        VerifyTestArray(reinterpret_cast<const float *>(RefData.data()),
                        reinterpret_cast<const float *>(TestData.data()),
                        NumArrayElems);
        break;
      case SERAccessor::UINT:
        VerifyTestArray(reinterpret_cast<const int *>(RefData.data()),
                        reinterpret_cast<const int *>(TestData.data()),
                        NumArrayElems);
        break;
      }
    }
  }
}

TEST_F(ExecutionTest, SERAttributesPermutationTest) {
  // SER: Test basic function of HitObject getters.

  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // All test variatinos
  SERTestConfig RefConfig = {true,
                             true,
                             false,
                             SERTestConfig::RayGen,
                             SERTestConfig::TraceRay,
                             SERTestConfig::FromShaders,
                             SERTestConfig::TraceRay};

  std::vector<SERTestConfig> TestConfigs;
  for (SERTestConfig::TestLocation TestLoc :
       {SERTestConfig::RayGen, SERTestConfig::Miss,
        SERTestConfig::ClosestHit}) {
    for (bool Reorder : {true, false}) {
      // MaybeReorderThreads only supported in RayGens
      if (TestLoc != SERTestConfig::RayGen && Reorder)
        continue;

      for (SERTestConfig::Method TestMethod :
           {SERTestConfig::HitObject_TraceRay, SERTestConfig::RayQuery}) {
        for (SERTestConfig::ResultFrom ResultSrc :
             {SERTestConfig::FromShaders, SERTestConfig::FromHitObject}) {
          SERTestConfig TestConfig = RefConfig;
          TestConfig.TestLoc = TestLoc;
          TestConfig.TraceMethod = TestMethod;
          TestConfig.ReorderHitObject = Reorder;
          TestConfig.ResultSrc = ResultSrc;

          if (TestLoc == SERTestConfig::RayGen) {
            TestConfigs.push_back(TestConfig);
            continue;
          }

          // Variations on primary shading call to test HitObject in CH/MS
          for (SERTestConfig::Method RecMethod :
               {SERTestConfig::TraceRay, SERTestConfig::HitObject_Invoke}) {
            TestConfig.RecMethod = RecMethod;
            TestConfigs.push_back(TestConfig);
          }
        }
      }
    }
  }

  // 64 x 64 test window size
  const int WindowSize = 64;

  const int NumResultElements = 2; // Just for Attrs
  const int RefMaxRecursion = RefConfig.hasRecursion() ? 2 : 1;

  std::vector<LPCWSTR> BaseArgs;
  BaseArgs.push_back(L"-HV 2021");
  BaseArgs.push_back(L"-Vd");
  BaseArgs.push_back(L"-DSCALAR_TYPE=float");
  BaseArgs.push_back(L"-DRESULT_TYPE=float2");
  BaseArgs.push_back(L"-DM_ROWS=2");
  BaseArgs.push_back(L"-DM_COLS=0");
  BaseArgs.push_back(L"-DATTRIBUTES_TEST=1");

  // Query reference result
  std::vector<int> RefData(WindowSize * WindowSize * NumResultElements);
  std::vector<LPCWSTR> RefArgs(BaseArgs);
  RefConfig.addCompileArgs(RefArgs);

  DXRRunConfig RunConfig = {
      WindowSize,
      WindowSize,
      RefConfig.UseTriangles,
      RefConfig.UseProceduralGeometry,
      RefMaxRecursion,
  };
  RunDXRTest(Device, SERPermutationTestShaderSrc, L"lib_6_9", RefArgs.data(),
             (int)RefArgs.size(), RefData, RunConfig);

  // Test permutations
  for (const auto &TestConfig : TestConfigs) {
    DXRRunConfig TestRunConfig(RunConfig);
    TestRunConfig.MaxRecursion = TestConfig.hasRecursion() ? 2 : 1;

    std::wstring TestConfigTxt =
        L"HitObject attributes with config " + TestConfig.str();

    {
      std::wstring TestingMsg = L"Testing " + TestConfigTxt;
      WEX::Logging::Log::Comment(TestingMsg.c_str());
    }

    std::vector<LPCWSTR> Args(BaseArgs);
    TestConfig.addCompileArgs(Args);

    std::vector<int> TestData(WindowSize * WindowSize * NumResultElements, 0);

    RunDXRTest(Device, SERPermutationTestShaderSrc, L"lib_6_9", Args.data(),
               (int)Args.size(), TestData, TestRunConfig);

    const int NumArrayElems = WindowSize * WindowSize * NumResultElements;
    VerifyTestArray(reinterpret_cast<const float *>(RefData.data()),
                    reinterpret_cast<const float *>(TestData.data()),
                    NumArrayElems);
  }
}

TEST_F(ExecutionTest, SERNOPValuesTest) {
  // SER: Test NOP HitObject default values
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

RWStructuredBuffer<int> testBuffer : register(u0);
RaytracingAccelerationStructure topObject : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);

float getFloatZero() { return 0.0f; }
int getIntZero() { return 0; }
int getIntOne() { return 1; }
float3 getVec3Zero() { return (float3)0; }

struct [raypayload] PerRayData
{
  int unused : read() : write();
};

#ifdef MATRIX_ELEMENT_TYPE
matrix<float, M_ROWS, M_COLS> getOneDiagonalMat() {
  matrix<float, M_ROWS, M_COLS> mat = 0;
  mat[0][0] = 1.f;
  mat[1][1] = 1.f;
  mat[2][2] = 1.f;
  return mat;
}
#endif

#if TEST_ATTRIBUTES
struct CustomAttrs {
  uint x;
  uint y;
  uint z;
  uint w;
};
#endif

[shader("raygeneration")]
void raygen()
{
   dx::HitObject hitObject = dx::HitObject::MakeNop();
#if TEST_ATTRIBUTES
  CustomAttrs attrs;
#if NEW_GETATTRIBUTES_API
  hitObject.GetAttributes<CustomAttrs>(attrs);
#else
  attrs = hitObject.GetAttributes<CustomAttrs>();
#endif
  testBuffer[0] = attrs.x;
  testBuffer[1] = attrs.y;
  testBuffer[2] = attrs.z;
  testBuffer[3] = attrs.w;
#else
   const bool pass = hitObject.HITOBJECT_GET_RESULT() == NOP_GET_RESULT();
   testBuffer[0] = pass ? 1 : 0;
   PerRayData pld;
   dx::HitObject::Invoke(hitObject, pld);
#endif
}


[shader("miss")]
void miss(inout PerRayData payload)
{
  testBuffer[1] = 1;
}

[shader("anyhit")]
void anyhit(inout PerRayData payload, in BuiltInTriangleIntersectionAttributes attrs)
{
    testBuffer[3] = 1;
}

[shader("closesthit")]
void closesthit(inout PerRayData payload, in BuiltInTriangleIntersectionAttributes attrs)
{
    testBuffer[2] = 1;
}


)";
  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  // Test GetAttributes<> on NOP HitObject
  {
    WEX::Logging::Log::Comment(L"Testing NOPHitObject::GetAttributes");

    LPCWSTR Args[] = {
        L"-HV 2021",
        L"-Vd",
        L"-DTEST_ATTRIBUTES=1",
    };

    std::vector<int> TestData(4, 0);
    DXRRunConfig RunConfig = {1, 1, true, false, 1};
    RunConfig.AttributeCount = 4;
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args, (int)std::size(Args),
               TestData, RunConfig);

    // Expect zero-init of attribute structure
    VERIFY_ARE_EQUAL(TestData[0], 0);
    VERIFY_ARE_EQUAL(TestData[1], 0);
    VERIFY_ARE_EQUAL(TestData[2], 0);
    VERIFY_ARE_EQUAL(TestData[3], 0);
  }

  for (const auto &Accessor : Accessors) {
    std::wstring TestConfigTxt = L"NOPHitObject::";
    TestConfigTxt += Accessor.HitObjectGetter;

    {
      std::wstring TestingMsg = L"Testing " + TestConfigTxt;
      WEX::Logging::Log::Comment(TestingMsg.c_str());
    }

    std::vector<LPCWSTR> Args;
    std::vector<std::wstring> OwnedArgs;
    Args.push_back(L"-HV 2021");
    Args.push_back(L"-Vd");
    Accessor.addCompileArgs(OwnedArgs, Args);

    std::vector<int> TestData(4, 0);
    DXRRunConfig RunConfig = {1, 1, true, false, 1};
    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args.data(), (int)Args.size(),
               TestData, RunConfig);

    VERIFY_ARE_EQUAL(TestData[0], 1); // hitObject.GET == expected nop value
    VERIFY_ARE_EQUAL(TestData[1], 0); // miss NOT called
    VERIFY_ARE_EQUAL(TestData[2], 0); // closesthit NOT called
    VERIFY_ARE_EQUAL(TestData[3], 0); // anyhit NOT called
  }
}

TEST_F(ExecutionTest, SERMultiPayloadTest) {
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

RWStructuredBuffer<uint> testBuffer : register(u0);
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

// Procedural geometry for use by RayQuery and intersection shader
static const int ProceduralHitKind = 11;

struct CustomAttrs
{
  float dist;
};

bool evalIntersection(float3 objRayOrigin, float3 objRayDir, float rayTMax, float rayTMin, out CustomAttrs attrs, out float rayT)
{
  rayT = 0;
  // Intersection with circle on a plane (base, n, radius)
  // hitPos is intersection point with plane (base, n)
  float3 base = {0.0f,0.0f,0.5f};
  float3 n = normalize(float3(0.0f,0.5f,0.5f));
  float radius = 500.f;
  // Plane hit
  float t = dot(n, base - objRayOrigin) / dot(n, objRayDir);
  if (t > rayTMax || t < rayTMin) {
      return false;
  }
  float3 hitPos = objRayOrigin + t * objRayDir;
  float3 relHitPos = hitPos - base;
  // Circle hit
  float hitDist = length(relHitPos);
  if (hitDist > radius)
    return false;

  attrs.dist = hitDist;
  rayT = t;
  return true;
}

#if ENABLE_PAQS
#define READ_PAQS(X, ...) : read(X, __VA_ARGS__)
#define WRITE_PAQS(X, ...) : write(X, __VA_ARGS__)
#else
#define READ_PAQS(X, ...)
#define WRITE_PAQS(X, ...)
#endif

struct
#if ENABLE_PAQS
[raypayload]
#endif
PayloadA
{
  float unusedPad READ_PAQS(caller, anyhit, closesthit) WRITE_PAQS(anyhit, closesthit, caller);
  uint ahCounter READ_PAQS(caller,anyhit) WRITE_PAQS(anyhit,caller);
  float unusedPad2 READ_PAQS(caller) WRITE_PAQS(closesthit,miss);
  uint chCounter READ_PAQS(caller,closesthit) WRITE_PAQS(closesthit,caller);
#if ENABLE_RECURSION
  int recursionDepth READ_PAQS(caller,miss,closesthit) WRITE_PAQS(caller);
#endif
  uint aabbCHCounter READ_PAQS(caller,closesthit) WRITE_PAQS(closesthit,caller);
  uint aabbAHCounter READ_PAQS(caller,anyhit) WRITE_PAQS(anyhit,caller);
  uint missCounter READ_PAQS(caller,miss) WRITE_PAQS(miss,caller);
};

struct
#if ENABLE_PAQS
[raypayload]
#endif
PayloadB
{
  uint ahCounter READ_PAQS(caller,anyhit) WRITE_PAQS(anyhit,caller);
  float unusedPad READ_PAQS(caller, anyhit, closesthit) WRITE_PAQS(anyhit, closesthit, caller);
  float unusedPad2 READ_PAQS(caller) WRITE_PAQS(closesthit,miss);
  uint chCounter READ_PAQS(caller,closesthit) WRITE_PAQS(closesthit,caller);
  uint aabbCHCounter READ_PAQS(caller,closesthit) WRITE_PAQS(closesthit,caller);
  uint aabbAHCounter READ_PAQS(caller,anyhit) WRITE_PAQS(anyhit,caller);
  uint missCounter READ_PAQS(caller,miss) WRITE_PAQS(miss,caller);
#if ENABLE_RECURSION
  int recursionDepth READ_PAQS(caller,miss,closesthit) WRITE_PAQS(caller);
#endif
};

/// Result tracking
static const uint NumRayResults = 10;
static void storeRayResult(int resIdx, uint value) {
  uint2 launchIndex = DispatchRaysIndex().xy;
  uint2 launchDim = DispatchRaysDimensions().xy;
  int baseIdx = NumRayResults * (launchIndex.x + launchIndex.y * launchDim.x);
  testBuffer[baseIdx + resIdx] += value;
}

void RunTest(int recursionDepth)
{
  RayDesc baseRay = ComputeRay();

  PayloadA pldA;
#if ENABLE_RECURSION
  pldA.recursionDepth = recursionDepth;
#endif
  pldA.ahCounter = 0;
  pldA.chCounter = 0;
  pldA.aabbCHCounter = 0;
  pldA.aabbAHCounter = 0;
  pldA.missCounter = 0;

  // First HitObject::TraceRay()
  dx::HitObject hitA = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_TRIANGLES, 0xFF, 0, 1, 0, baseRay, pldA);

  // Second HitObject::TraceRay() while other HitObject is live
  PayloadB pldB;
#if ENABLE_RECURSION
  pldB.recursionDepth = recursionDepth;
#endif
  pldB.ahCounter = 0;
  pldB.chCounter = 0;
  pldB.aabbCHCounter = 0;
  pldB.aabbAHCounter = 0;
  pldB.missCounter = 0;
  RayDesc rayB = baseRay;
  rayB.Origin.x += 0.1f;
  dx::HitObject hitB = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, 1, 1, 1, rayB, pldB);

  // TraceRay() while HitObject is live
  TraceRay(topObject, RAY_FLAG_NONE, 0xFF, 0, 1, 0, baseRay, pldA);

  // Concurrent HitObject with complex control flow
  dx::HitObject loopHit;
  int dynamicBound = hitA.GetGeometryIndex();
  for (int i = 0; i < dynamicBound + 5; ++i) {
    RayDesc loopRay = baseRay;
    loopRay.Origin.y += 0.001f * i;
    loopHit = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_TRIANGLES, 0xFF, 1, 1, 1, loopRay, pldB);
#if !ENABLE_RECURSION
    dx::MaybeReorderThread(loopHit);
#endif
  }

  // Invoke all HitObject (repeatedly)
  loopHit.SetShaderTableIndex(0); // pldA <- pldB
  dx::HitObject::Invoke(loopHit, pldA);
  hitA.SetShaderTableIndex(1); // pldB <- pldA
  int differentDynamicBound = hitA.GetInstanceIndex();
  for (int i = 0; i < differentDynamicBound + 3; ++i) {
    dx::HitObject::Invoke(hitA, pldB);
  }
  dx::HitObject::Invoke(hitB, pldB);

  // Write individual counters to distinct result slots
  // PayloadA
  storeRayResult(0, pldA.ahCounter);
  storeRayResult(1, pldA.chCounter);
  storeRayResult(2, pldA.aabbCHCounter);
  storeRayResult(3, pldA.aabbAHCounter);
  storeRayResult(4, pldA.missCounter);
  // PayloadB
  storeRayResult(5, pldB.ahCounter);
  storeRayResult(6, pldB.chCounter);
  storeRayResult(7, pldB.aabbCHCounter);
  storeRayResult(8, pldB.aabbAHCounter);
  storeRayResult(9, pldB.missCounter);
}

[shader("raygeneration")]
void raygen()
{
#if ENABLE_RECURSION
  RayDesc ray = ComputeRay();
  PayloadA recPayload;
  recPayload.recursionDepth = 1;
  dx::HitObject missObject = dx::HitObject::MakeMiss(RAY_FLAG_NONE, 2, ray);
  dx::HitObject::Invoke(missObject, recPayload);

#else
  RunTest(1);
#endif
}


///// Miss shaders
[shader("miss")]
void miss(inout PayloadA payload)
{
  payload.missCounter++;
}

[shader("miss")]
void miss1(inout PayloadB payload)
{
  payload.missCounter++;
}

#if ENABLE_RECURSION
[shader("miss")]
void miss2(inout PayloadA payload)
{
  if (payload.recursionDepth == 1)
  {
    RunTest(payload.recursionDepth + 1);
    return;
  }
}
#endif

///// Triangle HitGroup 0
[shader("anyhit")]
void anyhit(inout PayloadA payload, in BuiltInTriangleIntersectionAttributes attrs)
{
  payload.ahCounter++;
}

[shader("closesthit")]
void closesthit(inout PayloadA payload, in BuiltInTriangleIntersectionAttributes attrs)
{
  payload.chCounter++;
}

///// Triangle HitGroup 1
[shader("anyhit")]
void anyhit1(inout PayloadB payload, in BuiltInTriangleIntersectionAttributes attrs)
{
  payload.ahCounter++;
}

[shader("closesthit")]
void closesthit1(inout PayloadB payload, in BuiltInTriangleIntersectionAttributes attrs)
{
  payload.chCounter++;
}


///// Procedural HitGroup 0
[shader("closesthit")]
void chAABB(inout PayloadA payload, in CustomAttrs customAttrs)
{
  payload.aabbCHCounter++;
}

[shader("anyhit")]
void ahAABB(inout PayloadA payload, in CustomAttrs attrs)
{
  payload.aabbAHCounter++;
}

[shader("intersection")]
void intersection()
{
  CustomAttrs attrs = {0};
  float rayT;
  if (evalIntersection(ObjectRayOrigin(), ObjectRayDirection(), RayTCurrent(), RayTMin(), attrs, rayT)) {
    ReportHit(rayT, ProceduralHitKind, attrs);
  }
}


///// Procedural HitGroup 1
[shader("closesthit")]
void chAABB1(inout PayloadB payload, in CustomAttrs customAttrs)
{
  payload.aabbCHCounter++;
}

[shader("anyhit")]
void ahAABB1(inout PayloadB payload, in CustomAttrs attrs)
{
  payload.aabbAHCounter++;
}

[shader("intersection")]
void intersection1()
{
  CustomAttrs attrs = {0};
  float rayT;
  if (evalIntersection(ObjectRayOrigin(), ObjectRayDirection(), RayTCurrent(), RayTMin(), attrs, rayT)) {
    ReportHit(rayT, ProceduralHitKind, attrs);
  }
}

)";
  CComPtr<ID3D12Device> Device;
  if (!CreateDXRDevice(&Device, D3D_SHADER_MODEL_6_9, true))
    return;

  struct PayloadTestConfig {
    bool EnablePAQs;
    bool EnableRecursion;

    void addCompileArgs(std::vector<std::wstring> &OwnedArgs,
                      std::vector<LPCWSTR> &ArgVec) const {
      (void)  OwnedArgs;
      if (EnablePAQs) {
        ArgVec.push_back(L"-DENABLE_PAQS=1");
      } else {
        ArgVec.push_back(L"-disable-payload-qualifiers");
      }
      if (EnableRecursion) {
        ArgVec.push_back(L"-DENABLE_RECURSION=1");
      }
    }
  };

  // Expected histogram results for each result key, as {value, count} pairs.
  static const std::map<int, int> ExpectedResults[10] = {
      // result key 0
      {{0, 4060}, {1, 36}},
      // result key 1
      {{0, 847}, {1, 3213}, {2, 36}},
      // result key 2
      {{0, 883}, {1, 3213}},
      // result key 3
      {{0, 847}, {2, 3249}},
      // result key 4
      {{0, 3249}, {2, 847}},
      // result key 5
      {{0, 4030}, {1, 66}},
      // result key 6
      {{0, 847}, {4, 3183}, {5, 66}},
      // result key 7
      {{0, 4096}},
      // result key 8
      {{0, 847}, {5, 3249}},
      // result key 9
      {{0, 66}, {1, 3183}, {4, 847}}};

  const int WindowSize = 64;
  const int NumRayResults = 10;

  std::vector<PayloadTestConfig> TestConfigs;
  for (bool EnablePAQs : {false, true}) {
    for (bool EnableRecursion : {false, true}) {
      PayloadTestConfig TestConfig;
      TestConfig.EnablePAQs = EnablePAQs;
      TestConfig.EnableRecursion = EnableRecursion;
      TestConfigs.push_back(TestConfig);
    }
  }

  for (const auto &TestConfig : TestConfigs) {
    std::vector<int> TestData(WindowSize * WindowSize * NumRayResults, 0);
    DXRRunConfig RunConfig = {WindowSize, WindowSize, true, true, 1};
    RunConfig.PayloadCount = 7 + TestConfig.EnableRecursion;
    RunConfig.NumMissShaders = 2 + TestConfig.EnableRecursion;
    RunConfig.NumHitGroups = 2;
    RunConfig.MaxRecursion = 1 + TestConfig.EnableRecursion;

    std::vector<LPCWSTR> Args;
    std::vector<std::wstring> OwnedArgs;
    Args.push_back(L"-HV 2021");
    Args.push_back(L"-Vd");
    TestConfig.addCompileArgs(OwnedArgs, Args);

    RunDXRTest(Device, ShaderSrc, L"lib_6_9", Args.data(), (int)Args.size(),
               TestData, RunConfig);

    for (int ResIdx = 0; ResIdx < NumRayResults; ++ResIdx) {
      std::map<int, int> Histo;
      for (int RayIdx = 0; RayIdx < WindowSize * WindowSize; ++RayIdx) {
        int Val = TestData[ResIdx + (NumRayResults * RayIdx)];
        ++Histo[Val];
      }
      for (auto [Key, Value] : Histo) {
        VERIFY_IS_TRUE(ExpectedResults[ResIdx].count(Key));
        const int ExpectedValue = ExpectedResults[ResIdx].at(Key);
        VERIFY_ARE_EQUAL(Value, ExpectedValue);
      }
    }
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
    CustomAttrs customAttrs;
#if NEW_GETATTRIBUTES_API
    hitObject.GetAttributes<CustomAttrs>(customAttrs);
#else
    customAttrs = hitObject.GetAttributes<CustomAttrs>();
#endif
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

    int cat = (launchIndex.x + launchIndex.y) % 4;

    // Use wave incoherence to decide how to create the HitObject
    if (cat == 1)
    {
        // Turn this into an expected miss by moving eye behind triangles
        ray.Origin.z -= 1000.0f;
        hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, 0, 0, 0, ray, payload);
    }
    else if (cat == 2)
    {
        hitObject = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, 0, 0, 0, ray, payload);
    }
    else if (cat == 3)
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
    float radius = 500.f;
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

  VERIFY_ARE_EQUAL(Histo.size(), 4);
  VERIFY_ARE_EQUAL(Histo[1], 1024); // nop
  VERIFY_ARE_EQUAL(Histo[2], 2243); // miss
  VERIFY_ARE_EQUAL(Histo[20], 16);  // triangle hit
  VERIFY_ARE_EQUAL(Histo[40], 813); // procedural hit
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