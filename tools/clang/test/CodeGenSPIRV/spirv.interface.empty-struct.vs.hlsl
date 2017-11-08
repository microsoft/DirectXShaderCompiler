// Run: %dxc -T vs_6_0 -E main

// There is no interface variable for VSIn or VSOut empty structs.

// CHECK: OpEntryPoint Vertex %main "main" %gl_PerVertexOut

struct VSIn {};

struct VSOut {};

VSOut main(VSIn input)
{
  VSOut result;
  return result;
}
