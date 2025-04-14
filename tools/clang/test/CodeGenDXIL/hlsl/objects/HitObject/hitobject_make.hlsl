// RUN: %dxc -T lib_6_9 -E main %s -fcgl | FileCheck %s --check-prefix FCGL
// RUN: %dxc -T lib_6_9 -E main %s -ast-dump-implicit | FileCheck %s --check-prefix AST

// AST: | |-CXXRecordDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit referenced class HitObject definition
// AST-NEXT: | | |-FinalAttr {{[^ ]+}} <<invalid sloc>> Implicit final
// AST-NEXT: | | |-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.9 0 0 ""
// AST-NEXT: | | |-HLSLHitObjectAttr {{[^ ]+}} <<invalid sloc>> Implicit
// AST-NEXT: | | |-FieldDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit h 'int'
// AST-NEXT: | | |-CXXConstructorDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> used HitObject 'void ()'
// AST-NEXT: | | | |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 358
// AST-NEXT: | | | `-HLSLCXXOverloadAttr {{[^ ]+}} <<invalid sloc>> Implicit

// AST: | | |-FunctionTemplateDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MakeMiss
// AST-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TResult
// AST-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TRayFlags
// AST-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TMissShaderIndex
// AST-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TRay
// AST-NEXT: | | | |-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit MakeMiss 'TResult (TRayFlags, TMissShaderIndex, TRay) const' static
// AST-NEXT: | | | | |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> RayFlags 'TRayFlags'
// AST-NEXT: | | | | |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MissShaderIndex 'TMissShaderIndex'
// AST-NEXT: | | | | `-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> Ray 'TRay'
// AST-NEXT: | | | `-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> used MakeMiss 'dx::HitObject (unsigned int, unsigned int, RayDesc)' static
// AST-NEXT: | | |   |-TemplateArgument type 'dx::HitObject'
// AST-NEXT: | | |   |-TemplateArgument type 'unsigned int'
// AST-NEXT: | | |   |-TemplateArgument type 'unsigned int'
// AST-NEXT: | | |   |-TemplateArgument type 'RayDesc'
// AST-NEXT: | | |   |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MakeMiss 'unsigned int'
// AST-NEXT: | | |   |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> RayFlags 'unsigned int'
// AST-NEXT: | | |   |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MissShaderIndex 'RayDesc'
// AST-NEXT: | | |   |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 363
// AST-NEXT: | | |   `-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.9 0 0 ""

// AST: | | |-FunctionTemplateDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MakeNop
// AST-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TResult
// AST-NEXT: | | | |-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit MakeNop 'TResult () const' static
// AST-NEXT: | | | `-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> used MakeNop 'dx::HitObject ()' static
// AST-NEXT: | | |   |-TemplateArgument type 'dx::HitObject'
// AST-NEXT: | | |   |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 358
// AST-NEXT: | | |   `-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.9 0 0 ""

// FCGL: %1 = call %dx.types.HitObject* @"dx.hl.op..%dx.types.HitObject* (i32, %dx.types.HitObject*)"(i32 358, %dx.types.HitObject* %{{[^ ]+}})
// FCGL: call void @"dx.hl.op..void (i32, %dx.types.HitObject*)"(i32 358, %dx.types.HitObject* %{{[^ ]+}})
// FCGL: call void @"dx.hl.op..void (i32, %dx.types.HitObject*, i32, i32, %struct.RayDesc*)"(i32 363, %dx.types.HitObject* %{{[^ ]+}}, i32 0, i32 1, %struct.RayDesc* %{{[^ ]+}})

[shader("raygeneration")]
void main() {
  dx::HitObject hit;
  dx::HitObject::MakeNop();
  RayDesc ray = {{0,0,0}, {0,0,1}, 0.05, 1000.0};
  dx::HitObject::MakeMiss(0, 1, ray);
}
