// RUN: %dxc -E main -T ps_6_0 %s
// TODO: No check lines found, we should update this

Buffer<float4> g_buf;

[shader("pixel")]
float4 main() : SV_Target {
  return g_buf.Load(0);
}
