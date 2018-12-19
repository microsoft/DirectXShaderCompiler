// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: attribute 'unroll' must have a uint literal argument
// CHECK-NOT: @main

AppendStructuredBuffer<float> buf0;
AppendStructuredBuffer<float> buf1;
AppendStructuredBuffer<float> buf2;

uint g_cond;
uint g_cond2;
float main() : SV_Target {

  AppendStructuredBuffer<float> buf[3] = {
    buf0, buf1, buf2
  };

  [unroll(-1)]
  for (int i = 0; i < g_cond; i++) {
    if (i == g_cond2) {
      buf[i].Append(i);
      return 1;
    }
  }

  return 0;
}

