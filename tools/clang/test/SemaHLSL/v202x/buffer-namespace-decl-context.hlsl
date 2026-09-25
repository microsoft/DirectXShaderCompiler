// RUN: %dxc -T vs_6_3 -HV 202x -verify %s
// RUN: %dxc -T vs_6_3 -HV 2021 -verify %s
// RUN: %dxc -T vs_6_3 -HV 202x -verify -DLEGACY %s
// RUN: %dxc -T vs_6_3 -HV 2021 -verify -DLEGACY %s
// RUN: %dxc -T vs_6_3 -HV 202x -ast-dump %s 2>&1 | FileCheck %s

namespace First {
// CHECK: NamespaceDecl {{.*}} First
// CHECK-NEXT: {{.*}}HLSLBufferDecl {{0x[0-9a-f]+}} <{{.*}}> {{.*}} cbuffer Constants
cbuffer Constants {
  #if __HLSL_VERSION > 2021 && defined(LEGACY)
  //expected-note@+4{{'First::FirstValue' declared here}}
  #elif __HLSL_VERSION <= 2021 && !defined(LEGACY)
  //expected-note@+2{{'FirstValue' declared here}}
  #endif
  float FirstValue;
}

// CHECK: HLSLBufferDecl {{0x[0-9a-f]+}} <{{.*}}> {{.*}} tbuffer Textures
tbuffer Textures {
  #if __HLSL_VERSION > 2021 && defined(LEGACY)
  //expected-note@+4{{'First::FirstTextureValue' declared here}}
  #elif __HLSL_VERSION <= 2021 && !defined(LEGACY)
  //expected-note@+2{{'FirstTextureValue' declared here}}
  #endif
  float FirstTextureValue;
}
}

namespace Second {
// CHECK: NamespaceDecl {{.*}} Second
// CHECK-NEXT: {{.*}}HLSLBufferDecl {{0x[0-9a-f]+}} <{{.*}}> {{.*}} cbuffer Constants
cbuffer Constants {
  #if __HLSL_VERSION > 2021 && defined(LEGACY)
  //expected-note@+4{{'Second::SecondValue' declared here}}
  #elif __HLSL_VERSION <= 2021 && !defined(LEGACY)
  //expected-note@+2{{'SecondValue' declared here}}
  #endif
  float SecondValue;
}


// CHECK: HLSLBufferDecl {{0x[0-9a-f]+}} <{{.*}}> {{.*}} tbuffer Textures
tbuffer Textures {
  #if __HLSL_VERSION > 2021 && defined(LEGACY)
  //expected-note@+4{{'Second::SecondTextureValue' declared here}}
  #elif __HLSL_VERSION <= 2021 && !defined(LEGACY)
  //expected-note@+2{{'SecondTextureValue' declared here}}
  #endif
  float SecondTextureValue;
}
}

#ifndef LEGACY
#if __HLSL_VERSION > 2021
// expected-no-diagnostics
#else
//expected-error@+6{{no member named 'FirstValue' in namespace 'First'; did you mean simply 'FirstValue'?}}
//expected-error@+5{{no member named 'FirstTextureValue' in namespace 'First'; did you mean simply 'FirstTextureValue'?}}
//expected-error@+5{{no member named 'SecondValue' in namespace 'Second'; did you mean simply 'SecondValue'?}}
//expected-error@+4{{no member named 'SecondTextureValue' in namespace 'Second'; did you mean simply 'SecondTextureValue'?}}
#endif
float4 main() : SV_Target {
  return First::FirstValue + First::FirstTextureValue +
         Second::SecondValue + Second::SecondTextureValue;
}
#else
// This is how the code would have been written in HLSL 2021, and it now
// produces errors.
#if __HLSL_VERSION <= 2021
// expected-no-diagnostics
#else
//expected-error@+6{{use of undeclared identifier 'FirstValue'; did you mean 'First::FirstValue'?}}
//expected-error@+5{{use of undeclared identifier 'FirstTextureValue'; did you mean 'First::FirstTextureValue'?}}
//expected-error@+5{{use of undeclared identifier 'SecondValue'; did you mean 'Second::SecondValue'?}}
//expected-error@+4{{use of undeclared identifier 'SecondTextureValue'; did you mean 'Second::SecondTextureValue'}}
#endif
float4 main() : SV_Target {
  return FirstValue + FirstTextureValue +
         SecondValue + SecondTextureValue;
}
#endif
