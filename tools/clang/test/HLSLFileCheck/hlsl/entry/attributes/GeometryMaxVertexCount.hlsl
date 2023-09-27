// RUN: %dxc -T lib_6_3 %s | FileCheck %s

// CHECK: error: geometry entry point must have the maxvertexcount attribute

struct VSOut {
  float4 pos : SV_Position;
};

//[maxvertexcount(3)]
[shader("geometry")]
void main(inout TriangleStream<VSOut> stream) {
  VSOut v = {0.0, 0.0, 0.0, 0.0};
  stream.Append(v);
  stream.RestartStrip();
}
