// Run: %dxc -T ps_6_0 -E main

struct SBuffer {
  float4   f1;
  float2x3 f2[3];
};

  StructuredBuffer<SBuffer> mySBuffer1;
RWStructuredBuffer<SBuffer> mySBuffer2;

void main() {
  uint numStructs, stride;

// CHECK:       [[sb1:%\d+]] = OpLoad %type_StructuredBuffer_SBuffer %mySBuffer1
// CHECK-NEXT: [[len1:%\d+]] = OpArrayLength %uint [[sb1]] 0
// CHECK-NEXT:                 OpStore %numStructs [[len1]]
// CHECK-NEXT:                 OpStore %stride %uint_96
  mySBuffer1.GetDimensions(numStructs, stride);

// CHECK:       [[sb2:%\d+]] = OpLoad %type_RWStructuredBuffer_SBuffer %mySBuffer2
// CHECK-NEXT: [[len2:%\d+]] = OpArrayLength %uint [[sb2]] 0
// CHECK-NEXT:                 OpStore %numStructs [[len2]]
// CHECK-NEXT:                 OpStore %stride %uint_96
  mySBuffer2.GetDimensions(numStructs, stride);
}
