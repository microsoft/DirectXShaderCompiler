// RUN: %dxilver 1.5 | %dxc -T vs_6_2 -E main %s | FileCheck %s

// CHECK: store should be on uav resource

StructuredBuffer<int> buf;
void main() { buf[0] = 0; }