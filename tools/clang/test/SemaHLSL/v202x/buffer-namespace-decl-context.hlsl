// RUN: %dxc -T lib_6_3 -HV 202x -verify %s
// RUN: %dxc -T lib_6_3 -HV 202x -ast-dump %s 2>&1 | FileCheck %s

// expected-no-diagnostics

namespace First {
// CHECK: NamespaceDecl {{.*}} First
// CHECK-NEXT: {{.*}}HLSLBufferDecl {{0x[0-9a-f]+}} <{{.*}}> {{.*}} cbuffer Constants
cbuffer Constants {
  float FirstValue;
}

// CHECK: HLSLBufferDecl {{0x[0-9a-f]+}} <{{.*}}> {{.*}} tbuffer Textures
tbuffer Textures {
  float FirstTextureValue;
}
}

namespace Second {
// CHECK: NamespaceDecl {{.*}} Second
// CHECK-NEXT: {{.*}}HLSLBufferDecl {{0x[0-9a-f]+}} <{{.*}}> {{.*}} cbuffer Constants
cbuffer Constants {
  float SecondValue;
}

// CHECK: HLSLBufferDecl {{0x[0-9a-f]+}} <{{.*}}> {{.*}} tbuffer Textures
tbuffer Textures {
  float SecondTextureValue;
}
}

float4 main() : SV_Target {
  return First::FirstValue + First::FirstTextureValue +
         Second::SecondValue + Second::SecondTextureValue;
}
