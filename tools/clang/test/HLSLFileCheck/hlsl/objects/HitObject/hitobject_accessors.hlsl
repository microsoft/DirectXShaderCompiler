// RUN: %dxc -T lib_6_9 -E main %s | FileCheck %s

// CHECK-DAG: %dx.types.HitObject = type { i8* }

// CHECK-DAG:   %[[PLD:[^ ]+]] = alloca %struct.Payload, align 4
// CHECK:   %[[NOP:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeNop(i32 262)  ; HitObject_MakeNop()
// CHECK:   %[[HIT:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_SetShaderTableIndex(i32 285, %dx.types.HitObject %[[NOP]], i32 1)  ; HitObject_SetShaderTableIndex(hitObject,shaderTableIndex)
// CHECK-DAG:   %{{[^ ]+}} = call i1 @dx.op.hitObject_StateScalar.i1(i32 266, %dx.types.HitObject %[[HIT]])  ; HitObject_IsHit(hitObject)
// CHECK-DAG:   %{{[^ ]+}} = call i1 @dx.op.hitObject_StateScalar.i1(i32 265, %dx.types.HitObject %[[HIT]])  ; HitObject_IsMiss(hitObject)
// CHECK-DAG:   %{{[^ ]+}} = call i1 @dx.op.hitObject_StateScalar.i1(i32 267, %dx.types.HitObject %[[HIT]])  ; HitObject_IsNop(hitObject)
// CHECK-DAG:   %{{[^ ]+}} = call i32 @dx.op.hitObject_StateScalar.i32(i32 279, %dx.types.HitObject %[[HIT]])  ; HitObject_GeometryIndex(hitObject)
// CHECK-DAG:   %{{[^ ]+}} = call i32 @dx.op.hitObject_StateScalar.i32(i32 283, %dx.types.HitObject %[[HIT]])  ; HitObject_HitKind(hitObject)
// CHECK-DAG:   %{{[^ ]+}} = call i32 @dx.op.hitObject_StateScalar.i32(i32 280, %dx.types.HitObject %[[HIT]])  ; HitObject_InstanceIndex(hitObject)
// CHECK-DAG:   %{{[^ ]+}} = call i32 @dx.op.hitObject_StateScalar.i32(i32 281, %dx.types.HitObject %[[HIT]])  ; HitObject_InstanceID(hitObject)
// CHECK-DAG:   %{{[^ ]+}} = call i32 @dx.op.hitObject_StateScalar.i32(i32 282, %dx.types.HitObject %[[HIT]])  ; HitObject_PrimitiveIndex(hitObject)
// CHECK-DAG:   %{{[^ ]+}} = call i32 @dx.op.hitObject_StateScalar.i32(i32 284, %dx.types.HitObject %[[HIT]])  ; HitObject_ShaderTableIndex(hitObject)
// CHECK-DAG:   %{{[^ ]+}} = call i32 @dx.op.hitObject_LoadLocalRootTableConstant(i32 286, %dx.types.HitObject %[[HIT]], i32 42)  ; HitObject_LoadLocalRootTableConstant(hitObject,offset)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 273, %dx.types.HitObject %[[HIT]], i32 0)  ; HitObject_ObjectRayOrigin(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 273, %dx.types.HitObject %[[HIT]], i32 1)  ; HitObject_ObjectRayOrigin(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 273, %dx.types.HitObject %[[HIT]], i32 2)  ; HitObject_ObjectRayOrigin(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 274, %dx.types.HitObject %[[HIT]], i32 0)  ; HitObject_ObjectRayDirection(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 274, %dx.types.HitObject %[[HIT]], i32 1)  ; HitObject_ObjectRayDirection(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 274, %dx.types.HitObject %[[HIT]], i32 2)  ; HitObject_ObjectRayDirection(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 271, %dx.types.HitObject %[[HIT]], i32 0)  ; HitObject_WorldRayOrigin(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 271, %dx.types.HitObject %[[HIT]], i32 1)  ; HitObject_WorldRayOrigin(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 271, %dx.types.HitObject %[[HIT]], i32 2)  ; HitObject_WorldRayOrigin(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 272, %dx.types.HitObject %[[HIT]], i32 0)  ; HitObject_WorldRayDirection(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 272, %dx.types.HitObject %[[HIT]], i32 1)  ; HitObject_WorldRayDirection(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateVector.f32(i32 272, %dx.types.HitObject %[[HIT]], i32 2)  ; HitObject_WorldRayDirection(hitObject,component)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 0, i32 0)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 0, i32 1)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 0, i32 2)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 0, i32 3)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 1, i32 0)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 1, i32 1)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 1, i32 2)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 1, i32 3)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 2, i32 0)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 2, i32 1)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 2, i32 2)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 275, %dx.types.HitObject %[[HIT]], i32 2, i32 3)  ; HitObject_ObjectToWorld3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 0, i32 0)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 1, i32 0)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 2, i32 0)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 3, i32 0)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 0, i32 1)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 1, i32 1)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 2, i32 1)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 3, i32 1)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 0, i32 2)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 1, i32 2)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 2, i32 2)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 276, %dx.types.HitObject %[[HIT]], i32 3, i32 2)  ; HitObject_ObjectToWorld4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 0, i32 0)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 0, i32 1)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 0, i32 2)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 0, i32 3)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 1, i32 0)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 1, i32 1)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 1, i32 2)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 1, i32 3)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 2, i32 0)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 2, i32 1)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 2, i32 2)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 277, %dx.types.HitObject %[[HIT]], i32 2, i32 3)  ; HitObject_WorldToObject3x4(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 0, i32 0)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 1, i32 0)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 2, i32 0)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 3, i32 0)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 0, i32 1)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 1, i32 1)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 2, i32 1)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 3, i32 1)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 0, i32 2)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 1, i32 2)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 2, i32 2)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   %{{[^ ]+}} = call float @dx.op.hitObject_StateMatrix.f32(i32 278, %dx.types.HitObject %[[HIT]], i32 3, i32 2)  ; HitObject_WorldToObject4x3(hitObject,row,col)
// CHECK-DAG:   call void @dx.op.hitObject_Attributes.struct.AttribType(i32 287, %dx.types.HitObject %[[HIT]], %struct.AttribType* nonnull %{{[^ ]+}})  ; HitObject_Attributes(hitObject,attributes)
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[HIT]], %struct.Payload* nonnull %[[PLD]])  ; HitObject_Invoke(hitObject,payload)

struct [raypayload] Payload {
  float f : read(closesthit) : write(caller, anyhit);
  float i : read(closesthit) : write(caller, anyhit);
};

struct AttribType {
  float x;
  float y;
};

template <int M, int N>
float hashM(in matrix<float, M, N> mat) {
  float h = 0.f;
  for (int i = 0; i < M; ++i)
    for (int j = 0; j < N; ++j)
      h += mat[i][j];
  return h;
}

[shader("raygeneration")]
void main() {
  HitObject hit;
  int isum = 0;
  float fsum = 0.0f;
  vector<float, 3> vsum = 0;

  ///// Setters
  hit.SetShaderTableIndex(1);

  ///// Getters

  // i1 accessors
  isum += hit.IsHit();
  isum += hit.IsMiss();
  isum += hit.IsNop();

  // i32 accessors
  isum += hit.GetGeometryIndex();
  isum += hit.GetHitKind();
  isum += hit.GetInstanceIndex();
  isum += hit.GetInstanceID();
  isum += hit.GetPrimitiveIndex();
  isum += hit.GetShaderTableIndex();
  isum += hit.LoadLocalRootTableConstant(42);

  // float3 accessors
  vsum += hit.GetWorldRayOrigin();
  vsum += hit.GetWorldRayDirection();
  vsum += hit.GetObjectRayOrigin();
  vsum += hit.GetObjectRayDirection();
  fsum += vsum[0] + vsum[1] + vsum[2];

  // matrix accessors
  fsum += hashM<3, 4>(hit.GetObjectToWorld3x4());
  fsum += hashM<4, 3>(hit.GetObjectToWorld4x3());
  fsum += hashM<3, 4>(hit.GetWorldToObject3x4());
  fsum += hashM<4, 3>(hit.GetWorldToObject4x3());

  // attribute accessor
  AttribType A = hit.GetAttributes<AttribType>();
  fsum += A.x;
  fsum += A.y;

  // f32 accessors
  isum += hit.GetRayFlags();
  fsum += hit.GetRayTMin();
  fsum += hit.GetRayTCurrent();

  Payload pld;
  pld.f = fsum;
  pld.i = isum;
  HitObject::Invoke(hit, pld);
}
