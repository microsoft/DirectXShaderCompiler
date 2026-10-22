// RUN: %dxc -T lib_6_6 -HV 202x -ast-dump-implicit %s | FileCheck %s --implicit-check-not="UsingDirectiveDecl {{.*}} Namespace {{.*}} 'hlsl'"

// Verify that under HLSL 202x the implicit 'hlsl' namespace exists, that
// intrinsic functions used by the program are placed inside it, and that
// *no* implicit 'using namespace hlsl;' directive is injected.

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  float x;
  float3 v;
  float a = hlsl::sin(x);
  float b = hlsl::cos(x);
  float c = hlsl::dot(v, v);
}

// Translation unit should contain an implicit 'hlsl' namespace.
// CHECK: TranslationUnitDecl
// CHECK: NamespaceDecl {{.*}} implicit hlsl

// The intrinsic functions actually used by the shader should be parented to
// the 'hlsl' namespace.  Match the FunctionDecl entries that appear after the
// NamespaceDecl line above.
// CHECK: FunctionDecl {{.*}} implicit sin 'void ()' extern delete
// CHECK-NEXT: HLSLIntrinsicLookupMarkerAttr {{.*}} Implicit
// CHECK: FunctionDecl {{.*}} implicit used sin 'float (float)'
// CHECK-NOT: HLSLIntrinsicLookupMarkerAttr
// CHECK: FunctionDecl {{.*}} implicit cos 'void ()' extern delete
// CHECK-NEXT: HLSLIntrinsicLookupMarkerAttr {{.*}} Implicit
// CHECK: FunctionDecl {{.*}} implicit used cos 'float (float)'
// CHECK-NOT: HLSLIntrinsicLookupMarkerAttr
// CHECK: FunctionDecl {{.*}} implicit dot 'void ()' extern delete
// CHECK-NEXT: HLSLIntrinsicLookupMarkerAttr {{.*}} Implicit
// CHECK: FunctionDecl {{.*}} implicit used dot
// CHECK-NOT: HLSLIntrinsicLookupMarkerAttr

// No implicit 'using namespace hlsl;' directive is injected under 202x.
// CHECK-NOT: UsingDirectiveDecl {{.*}} Namespace {{.*}} 'hlsl'
