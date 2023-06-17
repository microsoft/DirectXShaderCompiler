// RUN: %dxc -T ps_6_0 /fcgl %s -opt-enable lifetime-markers | FileCheck %s -check-prefix=ENABLED
// RUN: %dxc -T ps_6_6 /fcgl %s -opt-disable lifetime-markers | FileCheck %s -check-prefix=DISABLED

// ENABLED: call void @llvm.lifetime.start
// DISABLED-NOT: @llvm.lifetime.start

[RootSignature("")]
float main(float input : INPUT) : SV_Target {
  float my_var = 0;
  my_var = input;
  return my_var;
}
