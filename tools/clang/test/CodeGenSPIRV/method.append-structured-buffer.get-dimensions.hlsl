// Run: %dxc -T vs_6_0 -E main

AppendStructuredBuffer<float4> buffer;

void main() {
  uint numStructs, stride;
  
// CHECK:      [[buf:%\d+]] = OpLoad %type_AppendStructuredBuffer_v4float %buffer
// CHECK-NEXT: [[len:%\d+]] = OpArrayLength %uint [[buf]] 0
// CHECK-NEXT: OpStore %numStructs [[len]]
// CHECK-NEXT: OpStore %stride %uint_16
  buffer.GetDimensions(numStructs, stride);
}
