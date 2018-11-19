// RUN: %dxc /T ps_6_0 /E main %s | FileCheck %s

// CHECK: error: bitfields are not supported in HLSL

struct Struct { uint field : 1; };
float main() : SV_Target { return 0; }