// RUN: %dxc -E main -T ps_6_0  %s | FileCheck %s

// Make sure generate ashr and lshr
// CHECK:ashr
// CHECK:lshr
int i;

int half_btf(int w,int bit) { return (w + (1<<bit)) >> bit; }
int half_btf2(int w,int bit) { return (w + (1U<<bit)) >> bit; }

int main(int w:W) : SV_Target {
  return half_btf(i,12) + half_btf2(i,12);
}