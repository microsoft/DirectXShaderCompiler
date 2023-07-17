// RUN: %dxc -T ps_6_0 -E main -fspv-debug=rich

struct foo {
  void method() { }
};

float4 main(float4 color : COLOR) : SV_TARGET {
  return color;
}

// CHECK: warning: Debug information for methods won't be linked to their class. See https://github.com/KhronosGroup/SPIRV-Registry/issues/203
