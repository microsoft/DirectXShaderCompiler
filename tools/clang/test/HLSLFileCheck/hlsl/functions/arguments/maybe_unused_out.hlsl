// RUN: %dxc -T lib_6_4 %s -ast-dump | FileCheck %s
// Verify attribute annotation to opt out of uninitialized parameter analysis.

void UnusedOutput([maybe_unused_out] out int Val) {}


// CHECK: FunctionDecl {{.*}} UnusedOutput 'void (int &__restrict)'
// CHECK-NEXT: ParmVarDecl {{0x[0-9a-fA-F]+}} <col:38, col:46> col:46 Val 'int &__restrict'
// CHECK-NEXT: HLSLOutAttr {{0x[0-9a-fA-F]+}} <col:38>
// CHECK-NEXT: HLSLMaybeUnusedOutAttr {{0x[0-9a-fA-F]+}} <col:20>
