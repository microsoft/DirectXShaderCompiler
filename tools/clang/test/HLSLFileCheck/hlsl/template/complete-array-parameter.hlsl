// RUN: %dxc -T lib_6_3 -ast-dump %s | FileCheck %s

float4 Val(Texture2D f[2]) {
  return float4(0,0,0,0);
}

// CHECK: FunctionDecl 0x{{[0-9a-fA-F]+}} <{{.*}}, line:5:1> line:3:8 Val 'float4 (Texture2D<vector<float, 4> > [2])'
// CHECK-NEXT: ParmVarDecl 0x{{[0-9a-fA-F]+}} <col:12, col:25> col:22 f 'Texture2D<vector<float, 4> > [2]'
