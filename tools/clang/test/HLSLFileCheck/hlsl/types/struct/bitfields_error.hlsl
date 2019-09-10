// RUN: %dxc /T vs_6_0 /E main %s | FileCheck %s

// CHECK: error: bitfields are not supported in HLSL

struct Struct { uint field : 1; };
void main() {}