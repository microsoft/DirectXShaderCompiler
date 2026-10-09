// RUN: %dxc -T cs_6_7 -E main -fcgl  %s -spirv | FileCheck %s

RWTexture2DMS<float4> t1 : register(u1);
RWTexture2DMSArray<int4> t2 : register(u2);
// A scalar sampled type takes a different image format and, on read, an
// extract out of the four-component texel.
RWTexture2DMSArray<uint, 8> t3 : register(u3);

[numthreads(1, 1, 1)]
void main() {
  uint2 coord = uint2(1, 2);
  uint3 coordArray = uint3(1, 2, 3);
  uint sampleIndex = 0;
  uint status;

  // t1.Load(Location, SampleIndex) reads one sample. Unlike the read-only
  // Texture2DMS(Array).Load(), there is no Offset parameter.
// CHECK:      [[img1:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4float [[img1]] {{%[0-9]+}} Sample {{%[0-9]+}}
  float4 a = t1.Load(coord, sampleIndex);

// CHECK:      [[img2:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4int [[img2]] {{%[0-9]+}} Sample {{%[0-9]+}}
  int4 b = t2.Load(coordArray, sampleIndex);

  // The three-argument form returns the operation status, so it reads
  // sparsely. The sample index still rides as the Sample image operand, and
  // the result is a struct the status and the texel are extracted from.
// CHECK:      [[imgS1:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: [[sp1:%[0-9]+]] = OpImageSparseRead {{%[a-zA-Z0-9_]+}} [[imgS1]] {{%[0-9]+}} Sample {{%[0-9]+}}
// CHECK-NEXT: {{%[0-9]+}} = OpCompositeExtract %uint [[sp1]] 0
  float4 c = t1.Load(coord, sampleIndex, status);

// CHECK:      [[imgS2:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: [[sp2:%[0-9]+]] = OpImageSparseRead {{%[a-zA-Z0-9_]+}} [[imgS2]] {{%[0-9]+}} Sample {{%[0-9]+}}
// CHECK-NEXT: {{%[0-9]+}} = OpCompositeExtract %uint [[sp2]] 0
  int4 cArray = t2.Load(coordArray, sampleIndex, status);

  // A scalar sampled type still reads a four-component texel and extracts.
// CHECK:      [[img3s:%[0-9]+]] = OpLoad %type_2d_image_array_0 %t3
// CHECK-NEXT: [[t3v:%[0-9]+]] = OpImageRead %v4uint [[img3s]] {{%[0-9]+}} Sample {{%[0-9]+}}
// CHECK-NEXT: {{%[0-9]+}} = OpCompositeExtract %uint [[t3v]] 0
  uint s1 = t3.Load(coordArray, sampleIndex);

  // A scalar write hands the scalar straight to OpImageWrite, with no splat to
  // four components. The texel here is a constant, so it is a named id.
// CHECK:      [[img3w:%[0-9]+]] = OpLoad %type_2d_image_array_0 %t3
// CHECK-NEXT: OpImageWrite [[img3w]] {{%[0-9]+}} %uint_7 Sample {{%[0-9]+}}
  t3.sample[sampleIndex][coordArray] = 7;

  // t.sample[idx][coord] reads one sample, same syntax as Texture2DMS.
// CHECK:      [[img3:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4float [[img3]] {{%[0-9]+}} Sample {{%[0-9]+}}
  float4 d = t1.sample[sampleIndex][coord];

// CHECK:      [[img4:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4int [[img4]] {{%[0-9]+}} Sample {{%[0-9]+}}
  int4 e = t2.sample[sampleIndex][coordArray];

  // The plain subscript carries no sample index, so it reads sample 0.
// CHECK:      [[img5:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4float [[img5]] {{%[0-9]+}} Sample %uint_0
  float4 f = t1[coord];

// CHECK:      [[img6:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: {{%[0-9]+}} = OpImageRead %v4int [[img6]] {{%[0-9]+}} Sample %uint_0
  int4 g = t2[coordArray];

  // t.sample[idx][coord] = value is the SM 6.7 writable-MSAA write path.
// CHECK:      [[img7:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: OpImageWrite [[img7]] {{%[0-9]+}} {{%[0-9]+}} Sample {{%[0-9]+}}
  t1.sample[sampleIndex][coord] = float4(1, 2, 3, 4);

// CHECK:      [[img8:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: OpImageWrite [[img8]] {{%[0-9]+}} {{%[0-9]+}} Sample {{%[0-9]+}}
  t2.sample[sampleIndex][coordArray] = int4(1, 2, 3, 4);

  // The plain subscript writes sample 0.
// CHECK:      [[img9:%[0-9]+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT: OpImageWrite [[img9]] {{%[0-9]+}} {{%[0-9]+}} Sample %uint_0
  t1[coord] = float4(5, 6, 7, 8);

// CHECK:      [[img10:%[0-9]+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT: OpImageWrite [[img10]] {{%[0-9]+}} {{%[0-9]+}} Sample %uint_0
  t2[coordArray] = int4(5, 6, 7, 8);

  // Writing one component reads the whole texel, inserts, and writes it back.
  // Both subscript forms have to reach the image-write path; a component write
  // that misses it stores through a non-pointer and fails validation.
// CHECK:      OpImageRead %v4float
// CHECK:      OpCompositeInsert %v4float
// CHECK:      OpImageWrite {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} Sample {{%[0-9]+}}
  t1.sample[sampleIndex][coord].x = 9.0;

// CHECK:      OpImageRead %v4int
// CHECK:      OpCompositeInsert %v4int
// CHECK:      OpImageWrite {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} Sample {{%[0-9]+}}
  t2.sample[sampleIndex][coordArray].y = 9;

// CHECK:      OpImageRead %v4float
// CHECK:      OpCompositeInsert %v4float
// CHECK:      OpImageWrite {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} Sample %uint_0
  t1[coord].z = 10.0;

  uint width, height, elements, samples;
// CHECK: OpImageQuerySize %v2uint {{%[0-9]+}}
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  t1.GetDimensions(width, height, samples);

// CHECK: OpImageQuerySize %v3uint {{%[0-9]+}}
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  t2.GetDimensions(width, height, elements, samples);

  // GetSamplePosition needs no resource-kind-specific handling; it queries
  // the sample count and indexes a table.
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  float2 p = t1.GetSamplePosition(sampleIndex);
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  float2 pArray = t2.GetSamplePosition(sampleIndex);

  // The float overloads of GetDimensions convert the integer query result.
  float fwidth, fheight, fsamples;
// CHECK: OpImageQuerySize %v2uint {{%[0-9]+}}
// CHECK: OpImageQuerySamples %uint {{%[0-9]+}}
  t1.GetDimensions(fwidth, fheight, fsamples);
}
