// RUN: %dxc -T lib_6_3 -DTYPE=float -Od %s | FileCheck %s -check-prefixes=CHECK
// RUN: %dxc -T lib_6_3 -DTYPE=float2x2 -Od %s | FileCheck %s -check-prefixes=CHECK
// RUN: %dxc -T lib_6_3 -DTYPE=float2x2 -Od -Zpr %s | FileCheck %s -check-prefixes=CHECK
// RUN: %dxc -T lib_6_3 -DTYPE=float2x2 -DTYPEMOD=row_major -Od -Zpr %s | FileCheck %s -check-prefixes=CHECK
// RUN: %dxc -T lib_6_3 -DTYPE=float2x2 -DTYPEMOD=row_major -Od -Zpr -fcgl %s | FileCheck %s -check-prefixes=FCGL_NOCOPY
// These need to copy:
// RUN: %dxc -T lib_6_3 -DTYPE=float2x2 -DTYPEMOD=row_major -Od %s | FileCheck %s -check-prefixes=CHECK,COPY
// RUN: %dxc -T lib_6_3 -DTYPE=float2x2 -DTYPEMOD=column_major -Od -Zpr %s | FileCheck %s -check-prefixes=CHECK,COPY

// Make sure array is not copied unless matrix orientation changed
// CHECK: alloca
// COPY: alloca
// CHECK-NOT: alloca
// CHECK: ret

// Make sure array is not copied in clang codeGen when orientation match but one is default orientation, one is explicit orientation.
// There should be only 1 alloca of matrix array for arr.
// FCGL_NOCOPY: define %class.matrix.float.2.2 @main()
// FCGL_NOCOPY: alloca [16 x %class.matrix.float.2.2]
// FCGL_NOCOPY-NOT: alloca [16 x %class.matrix.float.2.2]
// FCGL_NOCOPY: ret

#ifndef TYPEMOD
#define TYPEMOD
#endif

TYPE lookup(in TYPE arr[16], int index) {
  return arr[index];
}

int idx;

[shader("vertex")]
TYPE main() : OUT {
  TYPEMOD TYPE arr[16];
  for (int i = 0; i < 16; i++) {
    arr[i] = (TYPE)i;
  }
  return lookup(arr, idx);
}


