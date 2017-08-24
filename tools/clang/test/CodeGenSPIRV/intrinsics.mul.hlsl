// Run: %dxc -T ps_6_0 -E main

/*
According to HLSL reference, mul() has the following versions:

|Name|Purpose|Template|Component Type  |size|
|====|=======|========|================|==========================================================================|
|x   |in     |scalar  |float, int      |1                                                                         |
|y   |in     |scalar  |same as input x |1                                                                         |
|ret |out    |scalar  |same as input x |1                                                                         |
|====|=======|========|================|==========================================================================|
|x   |in     |scalar  |float, int      | 1                                                                        |
|y   |in     |vector  |float, int      |any                                                                       |
|ret |out    |vector  |float, int      |same dimension(s) as input y                                              |
|====|=======|========|================|==========================================================================|
|x   |in     |scalar  |float, int      |1                                                                         |
|y   |in     |matrix  |float, int      |any                                                                       |
|ret |out    |matrix  |same as intput y|same dimension(s) as input y                                              |
|====|=======|========|================|==========================================================================|
|x   |in     |vector  |float, int      |any                                                                       |
|y   |in     |scalar  |float, int      |1                                                                         |
|ret |out    |vector  |float, int      |same dimension(s) as input x                                              |
|====|=======|========|================|==========================================================================|
|x   |in     |vector  |float, int      |any                                                                       |
|y   |in     |vector  |float, int      |same dimension(s) as input x                                              |
|ret |out    |scalar  |float, int      |1                                                                         |
|====|=======|========|================|==========================================================================|
|x   |in     |vector  |float, int      |any                                                                       |
|y   |in     |matrix  |float, int      |rows = same dimension(s) as input x, columns = any                        |
|ret |out    |vector  |float, int      |same dimension(s) as input y columns                                      |
|====|=======|========|================|==========================================================================|
|x   |in     |matrix  |float, int      |any                                                                       |
|y   |in     |scalar  |float, int      |1                                                                         |
|ret |out    |matrix  |float, int      |same dimension(s) as input x                                              |
|====|=======|========|================|==========================================================================|
|x   |in     |matrix  |float, int      |any                                                                       |
|y   |in     |vector  |float, int      |number of columns in input x                                              |
|ret |out    |vector  |float, int      |number of rows in input x                                                 |
|====|=======|========|================|==========================================================================|
|x   |in     |matrix  |float, int      |any                                                                       |
|y   |in     |matrix  |float, int      |rows = number of columns in input x                                       |
|ret |out    |matrix  |float, int      |rows = number of rows in input x, columns = number of columns in input y  |
|====|=======|========|================|==========================================================================|
*/

void main() {

  float a, b;
// CHECK: {{%\d+}} = OpFMul %float {{%\d+}} {{%\d+}}
  float scalarMulscalar = mul(a,b);

  float float_c;
  float4 float4_d;

// CHECK:      [[float4_d:%\d+]] = OpLoad %v4float %float4_d
// CHECK-NEXT: [[float_c:%\d+]] = OpLoad %float %float_c
// CHECK-NEXT: {{%\d+}} = OpVectorTimesScalar %v4float [[float4_d]] [[float_c]]
  float4 float_scalarMulVector = mul(float_c,float4_d);

// CHECK:      [[float4_d1:%\d+]] = OpLoad %v4float %float4_d
// CHECK-NEXT: [[float_c1:%\d+]] = OpLoad %float %float_c
// CHECK-NEXT: {{%\d+}} = OpVectorTimesScalar %v4float [[float4_d1]] [[float_c1]]
  float4 float_vectorMulScalar = mul(float4_d,float_c);

  int int_c;
  int4 int4_d;

// CHECK:      [[int4_d:%\d+]] = OpLoad %v4int %int4_d
// CHECK-NEXT: [[int_c:%\d+]] = OpLoad %int %int_c
// CHECK-NEXT: [[c_splat:%\d+]] = OpCompositeConstruct %v4int [[int_c]] [[int_c]] [[int_c]] [[int_c]]
// CHECK-NEXT: {{%\d+}} = OpIMul %v4int [[c_splat]] [[int4_d]]
  int4 int_scalarMulVector = mul(int_c,int4_d);

// CHECK:      [[int4_d1:%\d+]] = OpLoad %v4int %int4_d
// CHECK-NEXT: [[int_c1:%\d+]] = OpLoad %int %int_c
// CHECK-NEXT: [[c_splat1:%\d+]] = OpCompositeConstruct %v4int [[int_c1]] [[int_c1]] [[int_c1]] [[int_c1]]
// CHECK-NEXT: {{%\d+}} = OpIMul %v4int [[int4_d1]] [[c_splat1]]
  int4 int_vectorMulScalar = mul(int4_d,int_c);
  
  float e;
  float3x4 f;

// CHECK:      [[e:%\d+]] = OpLoad %float %e
// CHECK-NEXT: [[f:%\d+]] = OpLoad %mat3v4float %f
// CHECK-NEXT: {{%\d+}} = OpMatrixTimesScalar %mat3v4float [[f]] [[e]]
  float3x4 scalarMulMatrix = mul(e,f);
  
// CHECK:      [[f1:%\d+]] = OpLoad %mat3v4float %f
// CHECK-NEXT: [[e1:%\d+]] = OpLoad %float %e
// CHECK-NEXT: {{%\d+}} = OpMatrixTimesScalar %mat3v4float [[f1]] [[e1]]
  float3x4 matrixMulScalar = mul(f,e);


  int4 g,h;
// CHECK:      [[g:%\d+]] = OpLoad %v4int %g
// CHECK-NEXT: [[h:%\d+]] = OpLoad %v4int %h
// CHECK-NEXT: [[g0:%\d+]] = OpCompositeExtract %int [[g]] 0
// CHECK-NEXT: [[h0:%\d+]] = OpCompositeExtract %int [[h]] 0
// CHECK-NEXT: [[g0h0:%\d+]] = OpIMul %int [[g0]] [[h0]]
// CHECK-NEXT: [[g1:%\d+]] = OpCompositeExtract %int [[g]] 1
// CHECK-NEXT: [[h1:%\d+]] = OpCompositeExtract %int [[h]] 1
// CHECK-NEXT: [[g1h1:%\d+]] = OpIMul %int [[g1]] [[h1]]
// CHECK-NEXT: [[g2:%\d+]] = OpCompositeExtract %int [[g]] 2
// CHECK-NEXT: [[h2:%\d+]] = OpCompositeExtract %int [[h]] 2
// CHECK-NEXT: [[g2h2:%\d+]] = OpIMul %int [[g2]] [[h2]]
// CHECK-NEXT: [[g3:%\d+]] = OpCompositeExtract %int [[g]] 3
// CHECK-NEXT: [[h3:%\d+]] = OpCompositeExtract %int [[h]] 3
// CHECK-NEXT: [[g3h3:%\d+]] = OpIMul %int [[g3]] [[h3]]
// CHECK-NEXT: [[add_1:%\d+]] = OpIAdd %int [[g0h0]] [[g1h1]]
// CHECK-NEXT: [[add_2:%\d+]] = OpIAdd %int [[add_1]] [[g2h2]]
// CHECK-NEXT: [[add_3:%\d+]] = OpIAdd %int [[add_2]] [[g3h3]]
// CHECK-NEXT: OpStore %vectorMulVector [[add_3]]
  int vectorMulVector = mul(g,h);

  float3 float_g, float_h;
// CHECK:      [[float_g:%\d+]] = OpLoad %v3float %float_g
// CHECK-NEXT: [[float_h:%\d+]] = OpLoad %v3float %float_h
// CHECK-NEXT: {{%\d+}} = OpDot %float [[float_g]] [[float_h]]
  float float_vectorMulVector = mul(float_g, float_h);

  float4 i;
  float4x3 j;
// CHECK:      [[i:%\d+]] = OpLoad %v4float %i
// CHECK-NEXT: [[j:%\d+]] = OpLoad %mat4v3float %j
// CHECK-NEXT: {{%\d+}} = OpMatrixTimesVector %v3float [[j]] [[i]]
  float3 vectorMulMatrix = mul(i,j);

  float2x3 k;
  float3 l;
// CHECK:      [[k:%\d+]] = OpLoad %mat2v3float %k
// CHECK-NEXT: [[l:%\d+]] = OpLoad %v3float %l
// CHECK-NEXT: {{%\d+}} = OpVectorTimesMatrix %v2float [[l]] [[k]]
  float2 matrixMulVector = mul(k,l);


  float3x4 m;
  float4x2 n;
// CHECK:      [[m:%\d+]] = OpLoad %mat3v4float %m
// CHECK-NEXT: [[n:%\d+]] = OpLoad %mat4v2float %n
// CHECK-NEXT: {{%\d+}} = OpMatrixTimesMatrix %mat3v2float [[n]] [[m]]
  float3x2 matrixMulMatrix = mul(m,n);
}
