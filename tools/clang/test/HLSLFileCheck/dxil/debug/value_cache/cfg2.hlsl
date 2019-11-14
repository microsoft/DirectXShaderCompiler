// RUN: %dxc -E main -T ps_6_0 -Od %s | FileCheck %s

// CHECK: @main

static bool gG;
static bool gG2;
static bool gG3;

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
Texture2D tex2 : register(t2);

Texture2D f(bool foo) {
  [branch]
  if (foo)
    return tex0;
  else
    return tex1;
}
Texture2D g(bool foo) {
  [branch]
  if (foo)
    return tex1;
  else
    return tex2;
}

Texture2D h(bool foo3) {
  return foo3 ? f(gG2) : g(gG3);
}

[RootSignature("DescriptorTable(SRV(t0, numDescriptors=3))")]
float4 main() : sv_target {
  gG = true;
  gG2 = false;
  gG3 = false;
  return h(gG).Load(0);
};


