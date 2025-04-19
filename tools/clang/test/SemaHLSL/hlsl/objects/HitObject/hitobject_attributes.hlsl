// RUN: %dxc -T lib_6_9 -E main %s -ast-dump-implicit | FileCheck %s --check-prefix AST
// RUN: %dxc -T lib_6_9 -E main %s -fcgl | FileCheck %s --check-prefix FCGL

// AST: | | |-FunctionTemplateDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> GetAttributes
// AST-NEXT: | | | |-TemplateTypeParmDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> class TResult
// AST-NEXT: | | | |-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit GetAttributes 'TResult () const'
// AST-NEXT: | | | `-CXXMethodDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> used GetAttributes 'CustomAttrs &()' extern
// AST-NEXT: | | |   |-TemplateArgument type 'CustomAttrs'
// AST-NEXT: | | |   |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 364
// AST-NEXT: | | |   `-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.9 0 0 ""

// FCGL: %{{[^ ]+}} = call %struct.CustomAttrs* @"dx.hl.op..%struct.CustomAttrs* (i32, %dx.types.HitObject*)"(i32 364, %dx.types.HitObject* %{{[^ ]+}})

RWByteAddressBuffer outbuf;

struct
CustomAttrs {
  float4 v;
  int y;
};

[shader("raygeneration")]
void main() {
  dx::HitObject hit;
  CustomAttrs attrs = hit.GetAttributes<CustomAttrs>();
  float sum = attrs.v.x + attrs.v.y + attrs.v.z + attrs.v.w + attrs.y;
  outbuf.Store(0, sum);
}
