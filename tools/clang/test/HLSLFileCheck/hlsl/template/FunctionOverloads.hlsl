// RUN: %dxc -T ps_6_6 -E main -HV 2021 -ast-dump %s | FileCheck %s

// CHECK: main
template <typename T> struct MyTex2D {
  uint heapId;

  template <typename Arg0> T Load(Arg0 arg0) { return Get().Load(arg0); }

  template <typename Arg0, typename Arg1> T Load(Arg0 arg0, Arg1 arg1) {
    return Get().Load(arg0, arg1);
  }

  Texture2D<T> Get() { return (Texture2D<T>)ResourceDescriptorHeap[heapId]; }
};

cbuffer constantBuffer : register(b0) { MyTex2D<float4> tex; };

float4 main() : SV_Target {
  float4 output = tex.Load(int3(0, 0, 0));
  return output;
}

// CHECK:      FunctionTemplateDecl {{0x[0-9a-fA-F]+}} <line:7:3, col:73> col:30 Load
// CHECK-NEXT: TemplateTypeParmDecl {{0x[0-9a-fA-F]+}} <col:13, col:22> col:22 typename Arg0
// CHECK-NEXT: CXXMethodDecl {{0x[0-9a-fA-F]+}} <col:28, col:73> col:30 Load 'vector<float, 4> (Arg0)'

// CHECK:      FunctionTemplateDecl {{0x[0-9a-fA-F]+}} <line:9:3, line:11:3> line:9:45 Load
// CHECK-NEXT: TemplateTypeParmDecl {{0x[0-9a-fA-F]+}} <col:13, col:22> col:22 typename Arg0
// CHECK-NEXT: TemplateTypeParmDecl {{0x[0-9a-fA-F]+}} <col:28, col:37> col:37 typename Arg1
// CHECK-NEXT: CXXMethodDecl {{0x[0-9a-fA-F]+}} <col:43, line:11:3> line:9:45 Load 'vector<float, 4> (Arg0, Arg1)'
