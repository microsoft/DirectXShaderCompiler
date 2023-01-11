// UNSUPPORTED: spirv

// RUN:not %dxc %s  /T ps_6_0 -spirv > %t.nospirv.log 2>&1
// RUN: FileCheck %s --input-file=%t.nospirv.log
// CHECK:SPIR-V CodeGen not available
