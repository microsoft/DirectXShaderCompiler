// RUN: %dxc -T cs_6_7 -E main -fcgl  %s -spirv | FileCheck %s

RWTexture2DMS<float4> t1 : register(u1);
RWTexture2DMSArray<int4> t2 : register(u2);
RWTexture2DMSArray<uint, 8> t3 : register(u3);

[numthreads(1, 1, 1)]
void main() {
  uint2 coord = uint2(1, 2);
  uint3 coordArray = uint3(1, 2, 3);
  uint sampleIndex = 1;
  uint layer = 3;
  uint status;

  // Load with a status argument does a sparse read of the given sample.
// CHECK:      [[imgS1:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: [[sp1:%[0-9]+]] = OpImageSparseRead {{%[a-zA-Z0-9_]+}} [[imgS1]] {{%[0-9]+}} Sample {{%[0-9]+}}
// CHECK-NEXT: {{%[0-9]+}} = OpCompositeExtract %uint [[sp1]] 0
  float4 c = t1.Load(coord, sampleIndex, status);

// CHECK:      [[imgS2:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: [[sp2:%[0-9]+]] = OpImageSparseRead {{%[a-zA-Z0-9_]+}} [[imgS2]] {{%[0-9]+}} Sample {{%[0-9]+}}
// CHECK-NEXT: {{%[0-9]+}} = OpCompositeExtract %uint [[sp2]] 0
  int4 cArray = t2.Load(coordArray, sampleIndex, status);

  // A scalar sampled type reads a four-component texel and extracts one.
// CHECK:      [[img3s:%[0-9]+]] = OpLoad %type_2d_image_array_0 %t3
// CHECK-NEXT: [[t3v:%[0-9]+]] = OpImageRead %v4uint [[img3s]] {{%[0-9]+}} Sample {{%[0-9]+}}
// CHECK-NEXT: {{%[0-9]+}} = OpCompositeExtract %uint [[t3v]] 0
  uint s1 = t3.Load(coordArray, sampleIndex);

  // A scalar write passes the scalar to OpImageWrite without a splat.
// CHECK:      [[img3w:%[0-9]+]] = OpLoad %type_2d_image_array_0 %t3
// CHECK-NEXT: OpImageWrite [[img3w]] {{%[0-9]+}} %uint_7 Sample {{%[0-9]+}}
  t3.sample[sampleIndex][coordArray] = 7;

  // .sample[idx][coord] reads the given sample.
// CHECK:      [[img3:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4float [[img3]] {{%[0-9]+}} Sample {{%[0-9]+}}
  float4 d = t1.sample[sampleIndex][coord];

// CHECK:      [[img4:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4int [[img4]] {{%[0-9]+}} Sample {{%[0-9]+}}
  int4 e = t2.sample[sampleIndex][coordArray];

  // The plain subscript reads sample 0.
// CHECK:      [[img5:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4float [[img5]] {{%[0-9]+}} Sample %uint_0
  float4 f = t1[coord];

// CHECK:      [[img6:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4int [[img6]] {{%[0-9]+}} Sample %uint_0
  int4 g = t2[coordArray];

  // The plain subscript writes sample 0.
// CHECK:      [[img9:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: OpImageWrite [[img9]] {{%[0-9]+}} {{%[0-9]+}} Sample %uint_0
  t1[coord] = float4(5, 6, 7, 8);

// CHECK:      [[img10:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: OpImageWrite [[img10]] {{%[0-9]+}} {{%[0-9]+}} Sample %uint_0
  t2[coordArray] = int4(5, 6, 7, 8);

  // A one-component write reads the texel, inserts the component and writes
  // the texel back to the same coordinate and sample.
// CHECK:      [[s1:%[0-9]+]] = OpLoad %uint %sampleIndex
// CHECK-NEXT: [[c1:%[0-9]+]] = OpLoad %v2uint %coord
// CHECK-NEXT: [[i1:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: [[old1:%[0-9]+]] = OpImageRead %v4float [[i1]] [[c1]] Sample [[s1]]
// CHECK-NEXT: [[new1:%[0-9]+]] = OpCompositeInsert %v4float %float_9 [[old1]] 0
// CHECK-NEXT: [[w1:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: OpImageWrite [[w1]] [[c1]] [[new1]] Sample [[s1]]
  t1.sample[sampleIndex][coord].x = 9.0;

// CHECK:      [[s2:%[0-9]+]] = OpLoad %uint %sampleIndex
// CHECK-NEXT: [[c2:%[0-9]+]] = OpLoad %v3uint %coordArray
// CHECK-NEXT: [[i2:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: [[old2:%[0-9]+]] = OpImageRead %v4int [[i2]] [[c2]] Sample [[s2]]
// CHECK-NEXT: [[new2:%[0-9]+]] = OpCompositeInsert %v4int %int_9 [[old2]] 1
// CHECK-NEXT: [[w2:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: OpImageWrite [[w2]] [[c2]] [[new2]] Sample [[s2]]
  t2.sample[sampleIndex][coordArray].y = 9;

// CHECK:      [[c3:%[0-9]+]] = OpLoad %v2uint %coord
// CHECK-NEXT: [[i3:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: [[old3:%[0-9]+]] = OpImageRead %v4float [[i3]] [[c3]] Sample %uint_0
// CHECK-NEXT: [[new3:%[0-9]+]] = OpCompositeInsert %v4float %float_10 [[old3]] 2
// CHECK-NEXT: [[w3:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: OpImageWrite [[w3]] [[c3]] [[new3]] Sample %uint_0
  t1[coord].z = 10.0;

  // The sample and coordinate of a one-component write are evaluated once,
  // and the read and the write use the same values.
// CHECK:      [[s4:%[0-9]+]] = OpLoad %uint %sampleIndex
// CHECK-NEXT: [[s4n:%[0-9]+]] = OpIAdd %uint [[s4]] %uint_1
// CHECK-NEXT: OpStore %sampleIndex [[s4n]]
// CHECK-NOT:  %sampleIndex
// CHECK:      OpImageRead %v4float {{%[0-9]+}} {{%[0-9]+}} Sample [[s4]]
// CHECK-NOT:  %sampleIndex
// CHECK:      OpImageWrite {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} Sample [[s4]]
  t1.sample[sampleIndex++][coord].x = 11.0;

// CHECK:      [[s5:%[0-9]+]] = OpLoad %uint %sampleIndex
// CHECK-NEXT: [[s5n:%[0-9]+]] = OpIAdd %uint [[s5]] %uint_1
// CHECK-NEXT: OpStore %sampleIndex [[s5n]]
// CHECK:      [[l5:%[0-9]+]] = OpLoad %uint %layer
// CHECK-NEXT: [[l5n:%[0-9]+]] = OpIAdd %uint [[l5]] %uint_1
// CHECK-NEXT: OpStore %layer [[l5n]]
// CHECK:      [[c5:%[0-9]+]] = OpCompositeConstruct %v3uint
// CHECK-NOT:  %sampleIndex
// CHECK-NOT:  %layer
// CHECK:      OpImageRead %v4int {{%[0-9]+}} [[c5]] Sample [[s5]]
// CHECK-NOT:  %sampleIndex
// CHECK-NOT:  %layer
// CHECK:      OpImageWrite {{%[0-9]+}} [[c5]] {{%[0-9]+}} Sample [[s5]]
  t2.sample[sampleIndex++][uint3(coord, layer++)].y = 12;

  // A compound assignment to one component computes from the sample it writes.
// CHECK:      [[s6:%[0-9]+]] = OpLoad %uint %sampleIndex
// CHECK-NEXT: [[s6n:%[0-9]+]] = OpIAdd %uint [[s6]] %uint_1
// CHECK-NEXT: OpStore %sampleIndex [[s6n]]
// CHECK-NEXT: [[c6:%[0-9]+]] = OpLoad %v2uint %coord
// CHECK-NOT:  %sampleIndex
// CHECK:      [[old6:%[0-9]+]] = OpImageRead %v4float {{%[0-9]+}} [[c6]] Sample [[s6]]
// CHECK-NEXT: [[x6:%[0-9]+]] = OpCompositeExtract %float [[old6]] 0
// CHECK:      [[sum6:%[0-9]+]] = OpFAdd %float [[x6]] %float_1
// CHECK-NOT:  %sampleIndex
// CHECK:      [[tex6:%[0-9]+]] = OpImageRead %v4float {{%[0-9]+}} [[c6]] Sample [[s6]]
// CHECK-NEXT: [[new6:%[0-9]+]] = OpCompositeInsert %v4float [[sum6]] [[tex6]] 0
// CHECK:      OpImageWrite {{%[0-9]+}} [[c6]] [[new6]] Sample [[s6]]
  t1.sample[sampleIndex++][coord].x += 1.0;

  uint width, height, elements, samples;
// CHECK: OpImageQuerySize %v2uint {{%[0-9]+}}
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  t1.GetDimensions(width, height, samples);

// CHECK: OpImageQuerySize %v3uint {{%[0-9]+}}
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  t2.GetDimensions(width, height, elements, samples);

  // GetSamplePosition queries the sample count.
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  float2 p = t1.GetSamplePosition(sampleIndex);
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  float2 pArray = t2.GetSamplePosition(sampleIndex);

  // The float GetDimensions overload queries the size and sample count.
  float fwidth, fheight, fsamples;
// CHECK: OpImageQuerySize %v2uint {{%[0-9]+}}
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  t1.GetDimensions(fwidth, fheight, fsamples);
}
