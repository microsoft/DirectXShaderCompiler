// RUN: %dxc -E main -T ps_6_0 -Od -Zi %s | FileCheck %s

// CHECK: llvm.dbg.declare

float4 main(float4 c : C) : SV_TARGET {
       float4 a = c;
       return a;
}
