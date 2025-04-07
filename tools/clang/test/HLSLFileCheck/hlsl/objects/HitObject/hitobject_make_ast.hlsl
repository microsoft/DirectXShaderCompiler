// RUN: %dxc -T lib_6_9 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: | |-CXXRecordDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit referenced class HitObject definition
// CHECK-NEXT: | | |-FinalAttr {{[^ ]+}} <<invalid sloc>> Implicit final
// CHECK-NEXT: | | |-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.9 0 0 ""
// CHECK-NEXT: | | |-HLSLHitObjectAttr {{[^ ]+}} <<invalid sloc>> Implicit
// CHECK-NEXT: | | |-FieldDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit h 'int'
// CHECK-NEXT: | | |-CXXConstructorDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> used HitObject 'void ()'
// CHECK-NEXT: | | | |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 358
// CHECK-NEXT: | | | `-HLSLCXXOverloadAttr {{[^ ]+}} <<invalid sloc>> Implicit

// CHECK: | | |-FunctionTemplateDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MakeMiss
// CHECK-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TResult
// CHECK-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TRayFlags
// CHECK-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TMissShaderIndex
// CHECK-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TRay
// CHECK-NEXT: | | | |-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit MakeMiss 'TResult (TRayFlags, TMissShaderIndex, TRay) const' static
// CHECK-NEXT: | | | | |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> RayFlags 'TRayFlags'
// CHECK-NEXT: | | | | |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MissShaderIndex 'TMissShaderIndex'
// CHECK-NEXT: | | | | `-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> Ray 'TRay'
// CHECK-NEXT: | | | `-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> used MakeMiss 'dx::HitObject (unsigned int, unsigned int, RayDesc)' static
// CHECK-NEXT: | | |   |-TemplateArgument type 'dx::HitObject'
// CHECK-NEXT: | | |   |-TemplateArgument type 'unsigned int'
// CHECK-NEXT: | | |   |-TemplateArgument type 'unsigned int'
// CHECK-NEXT: | | |   |-TemplateArgument type 'RayDesc'
// CHECK-NEXT: | | |   |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MakeMiss 'unsigned int'
// CHECK-NEXT: | | |   |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> RayFlags 'unsigned int'
// CHECK-NEXT: | | |   |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MissShaderIndex 'RayDesc'
// CHECK-NEXT: | | |   |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 363
// CHECK-NEXT: | | |   `-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.9 0 0 ""

// CHECK: | | |-FunctionTemplateDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> MakeNop
// CHECK-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TResult
// CHECK-NEXT: | | | |-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit MakeNop 'TResult () const' static
// CHECK-NEXT: | | | `-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> used MakeNop 'dx::HitObject ()' static
// CHECK-NEXT: | | |   |-TemplateArgument type 'dx::HitObject'
// CHECK-NEXT: | | |   |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 358
// CHECK-NEXT: | | |   `-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.9 0 0 ""

[shader("raygeneration")]
void main() {
  dx::HitObject hit;
  dx::HitObject::MakeNop();
  RayDesc ray = {{0,0,0}, {0,0,1}, 0.05, 1000.0};
  dx::HitObject::MakeMiss(0, 1, ray);
}
