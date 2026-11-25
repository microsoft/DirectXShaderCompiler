// RUN: %dxc -T lib_6_6 -HV 2016 -ast-dump-implicit %s | FileCheck %s --implicit-check-not=UsingDirectiveDecl
// RUN: %dxc -T lib_6_6 -HV 2017 -ast-dump-implicit %s | FileCheck %s --implicit-check-not=UsingDirectiveDecl
// RUN: %dxc -T lib_6_6 -HV 2018 -ast-dump-implicit %s | FileCheck %s --implicit-check-not=UsingDirectiveDecl
// RUN: %dxc -T lib_6_6 -HV 2021 -ast-dump-implicit %s | FileCheck %s --implicit-check-not=UsingDirectiveDecl

// Before HLSL 202x, the implicit 'hlsl' namespace is empty, intrinsics live
// directly in the translation unit, and no implicit using-directive is added.

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  float x;
  float a = sin(x);
}

// CHECK: TranslationUnitDecl
// CHECK: {{^\|-}}NamespaceDecl {{.*}} implicit hlsl
// CHECK-NEXT: {{^\|-}}ClassTemplateDecl {{.*}} implicit vector

// The intrinsic should still be declared, but as a direct child of the
// TranslationUnitDecl.
// CHECK: {{^[|`] *-}}FunctionDecl {{.*}} implicit used sin 'float (float)'
