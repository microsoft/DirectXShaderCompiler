// RUN: %dxc -T lib_6_6 -HV 2021 -ast-dump-implicit %s | FileCheck %s

// In HLSL 2021 (pre-202x), the built-in 'hlsl' namespace is NOT created and
// intrinsics live directly in the translation unit, so no NamespaceDecl
// named 'hlsl' should appear and no implicit using-directive should target
// the 'hlsl' namespace.

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  float x;
  float a = sin(x);
}

// CHECK-NOT: NamespaceDecl {{.*}} implicit hlsl
// CHECK-NOT: UsingDirectiveDecl {{.*}} 'hlsl'

// The intrinsic should still be declared, but as a direct child of the
// TranslationUnitDecl.
// CHECK: FunctionDecl {{.*}} implicit used sin 'float (float)'
