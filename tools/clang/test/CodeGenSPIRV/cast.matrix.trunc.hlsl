// Run: %dxc -T vs_6_0 -E main

// Note: Matrix truncation cast does not allow truncation into a mat1x1.

void main() {
  float3x4 m = { 1,  2,  3, 4,
                 5,  6,  7, 8,
                 9, 10, 11, 12 };

  float1x4 n = {1, 2, 3, 4};
  float3x1 o = {1, 2, 3};


// TEST: float3x4 --> float2x3

// CHECK:           [[m_0:%\d+]] = OpLoad %mat3v4float %m
// CHECK-NEXT: [[m_0_row0:%\d+]] = OpCompositeExtract %v4float [[m_0]] 0
// CHECK-NEXT:   [[a_row0:%\d+]] = OpVectorShuffle %v3float [[m_0_row0]] [[m_0_row0]] 0 1 2
// CHECK-NEXT: [[m_0_row1:%\d+]] = OpCompositeExtract %v4float [[m_0]] 1
// CHECK-NEXT:   [[a_row1:%\d+]] = OpVectorShuffle %v3float [[m_0_row1]] [[m_0_row1]] 0 1 2
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %mat2v3float [[a_row0]] [[a_row1]]
  float2x3 a = (float2x3)m;


// TEST: float3x4 --> float1x3

// CHECK:           [[m_1:%\d+]] = OpLoad %mat3v4float %m
// CHECK-NEXT: [[m_1_row0:%\d+]] = OpCompositeExtract %v4float [[m_1]] 0
// CHECK-NEXT:          {{%\d+}} = OpVectorShuffle %v3float [[m_1_row0]] [[m_1_row0]] 0 1 2
  float1x3 b = m;


// TEST: float3x4 --> float1x4

// CHECK:      [[m_2:%\d+]] = OpLoad %mat3v4float %m
// CHECK-NEXT:     {{%\d+}} = OpCompositeExtract %v4float [[m_2]] 0
  float1x4 c = m;


// TEST: float3x4 --> float2x1

// CHECK:           [[m_3:%\d+]] = OpLoad %mat3v4float %m
// CHECK-NEXT: [[m_3_row0:%\d+]] = OpCompositeExtract %v4float [[m_3]] 0
// CHECK-NEXT:   [[d_row0:%\d+]] = OpCompositeExtract %float [[m_3_row0]] 0
// CHECK-NEXT: [[m_3_row1:%\d+]] = OpCompositeExtract %v4float [[m_3]] 1
// CHECK-NEXT:   [[d_row1:%\d+]] = OpCompositeExtract %float [[m_3_row1]] 0
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %v2float [[d_row0]] [[d_row1]]
  float2x1 d = m;


// TEST: float3x4 --> float3x1

// CHECK:           [[m_4:%\d+]] = OpLoad %mat3v4float %m
// CHECK-NEXT: [[m_4_row0:%\d+]] = OpCompositeExtract %v4float [[m_4]] 0
// CHECK-NEXT:   [[e_row0:%\d+]] = OpCompositeExtract %float [[m_4_row0]] 0
// CHECK-NEXT: [[m_4_row1:%\d+]] = OpCompositeExtract %v4float [[m_4]] 1
// CHECK-NEXT:   [[e_row1:%\d+]] = OpCompositeExtract %float [[m_4_row1]] 0
// CHECK-NEXT: [[m_4_row2:%\d+]] = OpCompositeExtract %v4float [[m_4]] 2
// CHECK-NEXT:   [[e_row2:%\d+]] = OpCompositeExtract %float [[m_4_row2]] 0
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %v3float [[e_row0]] [[e_row1]] [[e_row2]]
  float3x1 e = (float3x1)m;


// TEST float1x4 --> float1x3

// CHECK:      [[n:%\d+]] = OpLoad %v4float %n
// CHECK-NEXT:   {{%\d+}} = OpVectorShuffle %v3float [[n]] [[n]] 0 1 2
  float1x3 f = n;


// TEST float3x1 --> float2x1

// CHECK:      [[o:%\d+]] = OpLoad %v3float %o
// CHECK-NEXT:   {{%\d+}} = OpVectorShuffle %v2float [[o]] [[o]] 0 1
  float2x1 g = (float2x1)o;
}
