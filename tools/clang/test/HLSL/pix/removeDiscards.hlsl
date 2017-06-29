// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-remove-discards | %FileCheck %s

// Check that the discard within the if/then was removed:
// CHECK: if.then:                                          ; preds = %entry
// CHECK:   br label %if.end
// CHECK: if.end:

struct RTOut
{
  int i : SV_Target;
  float4 c : SV_Target1;
};

[RootSignature("")]
RTOut main(float r : r, float g : g, float b : b, float a : a)  {
  r *= 2.f;
  g *= 4.f;
  b *= 8.f;
  a *= 16.f;
  if (r > 3.f)
  {
    discard;
  }
  RTOut rtOut;
  rtOut.i = 8;
  rtOut.c = float4(r,g,b,a);
  return rtOut;
}
