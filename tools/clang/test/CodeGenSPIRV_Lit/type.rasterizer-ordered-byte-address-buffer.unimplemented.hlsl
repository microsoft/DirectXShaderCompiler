// RUN: not %dxc -T ps_6_6 -E main -fcgl %s -spirv 2>&1 | FileCheck %s

// TODO: Once these are implemented, we will want to add tests checking that
//       RasterizerOrdered* types have all the functionality of RW* types

// CHECK: error: rasterizer ordered views are unimplemented
RasterizerOrderedByteAddressBuffer rob;

void main() { }
