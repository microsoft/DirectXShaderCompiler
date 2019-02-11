// RUN: %dxc -E main -T vs_6_2 %s | FileCheck %s

// Repro of GitHub #1916

void inc_i32x2(inout int2 val) { val++; }
void main(inout float2 f32x2 : F32X2) { inc_i32x2(f32x2); }