// RUN: not %dxc -T vs_6_2 -E main %s 2>&1 | FileCheck %s

// REQUIRES: dxilver_1_5

// CHECK: error: cannot assign to return value because function 'operator[]<const int &>' returns a const value

StructuredBuffer<int> buf;
void main() { buf[0] = 0; }