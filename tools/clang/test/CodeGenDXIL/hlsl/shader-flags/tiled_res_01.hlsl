// RUN: %dxc -T ps_6_8 %s | FileCheck %s

// From DXC disassembly comment:
// CHECK: Note: shader requires additional functionality:
// CHECK-NEXT: Tiled resources

// CHECK: !dx.entryPoints = !{![[entryPoints:[0-9]+]]}
// CHECK: ![[entryPoints]] = !{void ()* @main, !"main", !{{[0-9]+}}, !{{[0-9]+}}, ![[extAttr:[0-9]+]]}

// tag 0: ShaderFlags, 4096 = Tiled resources
// CHECK: ![[extAttr]] = !{i32 0, i64 4096}

Texture2D T2D;
SamplerComparisonState S;
float bias;
float cmp;
float4 main(float2 coord : TEXCOORD, float c : CLAMP) : SV_Target {
  // LOD clamp requires TiledResources feature
  return T2D.SampleCmpBias(S, coord, cmp, bias, int2(0,0), c);
}
