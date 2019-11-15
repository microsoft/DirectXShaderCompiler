// RUN: %dxc -T ps_6_0 -E main %s -validator-version 1.5 | FileCheck %s

// Make sure use uint isfrontface works.
// CHECK: main

float main(uint b : SV_IsFrontFace) : SV_Target {
  return b;
}