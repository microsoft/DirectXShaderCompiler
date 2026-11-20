// RUN: %dxc -T cs_6_0 -E main -Zi -Qembed_debug %s | FileCheck %s

// CHECK: !llvm.dbg.cu = !{![[CU:[0-9]+]]}
// CHECK: ![[CU]] = distinct !DICompileUnit(language: DW_LANG_HLSL,

[numthreads(1, 1, 1)]
void main() {}
