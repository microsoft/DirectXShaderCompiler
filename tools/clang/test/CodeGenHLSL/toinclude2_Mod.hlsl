// RUN: %dxc -E includedFunc2 -T ps_5_0 %s

float includedFunc2(int c : A) : SV_Target {
  return c + 100;
}