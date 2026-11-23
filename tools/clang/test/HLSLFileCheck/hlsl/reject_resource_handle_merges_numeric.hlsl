// RUN: %dxc -T ps_6_0 -E main -reject-resource-handle-merges %s | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -Od -reject-resource-handle-merges %s | FileCheck %s
// CHECK: define void @main
// CHECK: ret void

// The compatibility policy must allow ordinary numeric control flow.
float main(float value : VALUE) : SV_Target {
  float result = value > 0.0 ? value : -value;
  if (value > 1.0)
    result += value;
  else
    result *= value;
  return result;
}
