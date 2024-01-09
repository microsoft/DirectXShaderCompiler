// RUN: not %dxc -T ds_6_0 -E main %s 2>&1 | FileCheck %s
// RUN: not %dxc -T ds_6_0 -E main %s -spirv 2>&1 | FileCheck %s

struct ControlPoint {
  float position : MY_BOOL;
};

ControlPoint main(const OutputPatch<ControlPoint, 0> patch) {
  // CHECK: error: OutputPatch element count must be greater than 0
  return patch[0];
}

