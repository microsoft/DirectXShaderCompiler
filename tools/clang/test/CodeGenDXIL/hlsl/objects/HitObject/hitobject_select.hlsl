// RUN: %dxc -T lib_6_9 -E main %s | FileCheck %s --check-prefix=DXIL
// RUN: %dxc -T lib_6_9 -E main %s -fcgl | FileCheck %s --check-prefix=FCGL
// RUN: %dxc -T lib_6_9 -E main %s -ast-dump-implicit | FileCheck %s --check-prefix=AST

// AST: `-FunctionDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit used select 'dx::HitObject (bool, dx::HitObject, dx::HitObject)' extern
// AST-NEXT:   |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> cond 'bool'
// AST-NEXT:   |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> t 'dx::HitObject':'dx::HitObject'
// AST-NEXT:   |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> f 'dx::HitObject':'dx::HitObject'
// AST-NEXT:   |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 184
// AST-NEXT:   |-ConstAttr {{[^ ]+}} <<invalid sloc>> Implicit
// AST-NEXT:   |-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.9 0 0 ""
// AST-NEXT:   `-HLSLBuiltinCallAttr {{[^ ]+}} <<invalid sloc>> Implicit

// FCGL: %[[OBJA:[^ ]+]] = alloca %dx.types.HitObject, align 4
// FCGL: %[[OBJB:[^ ]+]] = alloca %dx.types.HitObject, align 4
// FCGL: %[[OBJC:[^ ]+]] = alloca %dx.types.HitObject, align 4
// FCGL: call void @"dx.hl.op..void (i32, %dx.types.HitObject*)"(i32 358, %dx.types.HitObject* %[[OBJA]])
// FCGL: call void @"dx.hl.op..void (i32, %dx.types.HitObject*, i32, i32, %struct.RayDesc*)"(i32 387, %dx.types.HitObject* %[[OBJB]]
// FCGL: call void @"dx.hl.op..void (i32, %dx.types.HitObject*, i1, %dx.types.HitObject*, %dx.types.HitObject*)"(i32 184, %dx.types.HitObject* %[[OBJC]], i1 %4, %dx.types.HitObject* %[[OBJA]], %dx.types.HitObject* %[[OBJB]])

// DXIL: %[[NOP:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeNop(i32 266)  ; HitObject_MakeNop()
// DXIL: %[[MISS:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeMiss(i32 265, i32 0, i32 0, float undef, float undef, float undef, float undef, float undef, float undef, float undef, float undef)  ; HitObject_MakeMiss(RayFlags,MissShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax)
// DXIL: %[[SELECT:[^ ]+]] = select i1 %{{[^ ]+}}, %dx.types.HitObject %[[NOP]], %dx.types.HitObject %[[MISS]]
// DXIL: call i1 @dx.op.hitObject_StateScalar.i1(i32 271, %dx.types.HitObject %[[SELECT]])  ; HitObject_IsNop(hitObject)

RWBuffer<uint> output : register(u0, space0);

[shader("raygeneration")]
void RayGen()
{
  RayDesc ray;
  dx::HitObject obja = dx::HitObject::MakeNop();
  dx::HitObject objb = dx::HitObject::MakeMiss(0, 0, ray);
  dx::HitObject objc = select(obja.IsMiss(), obja, objb);
  output[0] = objc.IsNop();
}