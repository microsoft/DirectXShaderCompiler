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

///////////////////////////////////////
/// Non-floating point matrix cases ///
///////////////////////////////////////

  uint  uintScalar;
  int   intScalar;
  float floatScalar;

  // Scalar * Matrix
// CHECK:        [[intScalar:%\d+]] = OpLoad %int %intScalar
// CHECK-NEXT:      [[intMat:%\d+]] = OpLoad %_arr_v3int_uint_2 %intMat2x3
// CHECK-NEXT: [[v3intScalar:%\d+]] = OpCompositeConstruct %v3int [[intScalar]] [[intScalar]] [[intScalar]]
// CHECK-NEXT:     [[intMat0:%\d+]] = OpCompositeExtract %v3int [[intMat]] 0
// CHECK-NEXT:        [[mul0:%\d+]] = OpIMul %v3int [[intMat0]] [[v3intScalar]]
// CHECK-NEXT:     [[intMat1:%\d+]] = OpCompositeExtract %v3int [[intMat]] 1
// CHECK-NEXT:        [[mul1:%\d+]] = OpIMul %v3int [[intMat1]] [[v3intScalar]]
// CHECK-NEXT:             {{%\d+}} = OpCompositeConstruct %_arr_v3int_uint_2 [[mul0]] [[mul1]]
  int2x3   intMat2x3;
  int2x3 o = mul(intScalar, intMat2x3);

  // Matrix * Scalar
// CHECK:           [[uintMat:%\d+]] = OpLoad %_arr_v3uint_uint_2 %uintMat2x3
// CHECK-NEXT:   [[uintScalar:%\d+]] = OpLoad %uint %uintScalar
// CHECK-NEXT: [[v3uintScalar:%\d+]] = OpCompositeConstruct %v3uint [[uintScalar]] [[uintScalar]] [[uintScalar]]
// CHECK-NEXT:     [[uintMat0:%\d+]] = OpCompositeExtract %v3uint [[uintMat]] 0
// CHECK-NEXT:         [[mul0:%\d+]] = OpIMul %v3uint [[uintMat0]] [[v3uintScalar]]
// CHECK-NEXT:     [[uintMat1:%\d+]] = OpCompositeExtract %v3uint [[uintMat]] 1
// CHECK-NEXT:         [[mul1:%\d+]] = OpIMul %v3uint [[uintMat1]] [[v3uintScalar]]
// CHECK-NEXT:              {{%\d+}} = OpCompositeConstruct %_arr_v3uint_uint_2 [[mul0]] [[mul1]]
  uint2x3  uintMat2x3;
  uint2x3 p = mul(uintMat2x3, uintScalar);

  // Matrix * Scalar (different types)
  // Casting AST nodes are inserted by the front-end. Mul works same as above.
// CHECK:           [[intMat:%\d+]] = OpLoad %_arr_v4int_uint_2 %intMat2x4
// CHECK-NEXT:     [[intMat0:%\d+]] = OpCompositeExtract %v4int [[intMat]] 0
// CHECK-NEXT:   [[floatMat0:%\d+]] = OpConvertSToF %v4float [[intMat0]]
// CHECK-NEXT:     [[intMat1:%\d+]] = OpCompositeExtract %v4int [[intMat]] 1
// CHECK-NEXT:   [[floatMat1:%\d+]] = OpConvertSToF %v4float [[intMat1]]
// CHECK-NEXT:    [[floatMat:%\d+]] = OpCompositeConstruct %mat2v4float [[floatMat0]] [[floatMat1]]
// CHECK-NEXT: [[floatScalar:%\d+]] = OpLoad %float %floatScalar
// CHECK-NEXT:             {{%\d+}} = OpMatrixTimesScalar %mat2v4float [[floatMat]] [[floatScalar]]
  int2x4 intMat2x4;
  float2x4 q = mul(intMat2x4, floatScalar);

  // Vector * Matrix
  // First, we need to get vectors for the columns of the matrix, and then perform
  // dot product of the vector and the matrix columns.
// CHECK:               [[intVec:%\d+]] = OpLoad %v2int %intVec2
// CHECK-NEXT:          [[intMat:%\d+]] = OpLoad %_arr_v3int_uint_2 %intMat2x3
// CHECK-NEXT:        [[intMat00:%\d+]] = OpCompositeExtract %int [[intMat]] 0 0
// CHECK-NEXT:        [[intMat01:%\d+]] = OpCompositeExtract %int [[intMat]] 0 1
// CHECK-NEXT:        [[intMat02:%\d+]] = OpCompositeExtract %int [[intMat]] 0 2
// CHECK-NEXT:        [[intMat10:%\d+]] = OpCompositeExtract %int [[intMat]] 1 0
// CHECK-NEXT:        [[intMat11:%\d+]] = OpCompositeExtract %int [[intMat]] 1 1
// CHECK-NEXT:        [[intMat12:%\d+]] = OpCompositeExtract %int [[intMat]] 1 2
// CHECK-NEXT:      [[intMatCol0:%\d+]] = OpCompositeConstruct %v2int [[intMat00]] [[intMat10]]
// CHECK-NEXT:      [[intMatCol1:%\d+]] = OpCompositeConstruct %v2int [[intMat01]] [[intMat11]]
// CHECK-NEXT:      [[intMatCol2:%\d+]] = OpCompositeConstruct %v2int [[intMat02]] [[intMat12]]
// CHECK-NEXT: [[intMatTranspose:%\d+]] = OpCompositeConstruct %_arr_v2int_uint_3 [[intMatCol0]] [[intMatCol1]] [[intMatCol2]]
// CHECK-NEXT:      [[intMatCol0:%\d+]] = OpCompositeExtract %v2int [[intMatTranspose]] 0
// CHECK-NEXT:         [[intVec0:%\d+]] = OpCompositeExtract %int [[intVec]] 0
// CHECK-NEXT:     [[intMatCol00:%\d+]] = OpCompositeExtract %int [[intMatCol0]] 0
// CHECK-NEXT:            [[mul1:%\d+]] = OpIMul %int [[intVec0]] [[intMatCol00]]
// CHECK-NEXT:         [[intVec1:%\d+]] = OpCompositeExtract %int [[intVec]] 1
// CHECK-NEXT:     [[intMatCol01:%\d+]] = OpCompositeExtract %int [[intMatCol0]] 1
// CHECK-NEXT:            [[mul2:%\d+]] = OpIMul %int [[intVec1]] [[intMatCol01]]
// CHECK-NEXT:              [[r0:%\d+]] = OpIAdd %int [[mul1]] [[mul2]]
// CHECK-NEXT:      [[intMatCol1:%\d+]] = OpCompositeExtract %v2int [[intMatTranspose]] 1
// CHECK-NEXT:         [[intVec0:%\d+]] = OpCompositeExtract %int [[intVec]] 0
// CHECK-NEXT:     [[intMatCol10:%\d+]] = OpCompositeExtract %int [[intMatCol1]] 0
// CHECK-NEXT:            [[mul3:%\d+]] = OpIMul %int [[intVec0]] [[intMatCol10]]
// CHECK-NEXT:         [[intVec1:%\d+]] = OpCompositeExtract %int [[intVec]] 1
// CHECK-NEXT:     [[intMatCol11:%\d+]] = OpCompositeExtract %int [[intMatCol1]] 1
// CHECK-NEXT:            [[mul4:%\d+]] = OpIMul %int [[intVec1]] [[intMatCol11]]
// CHECK-NEXT:              [[r1:%\d+]] = OpIAdd %int [[mul3]] [[mul4]]
// CHECK-NEXT:      [[intMatCol2:%\d+]] = OpCompositeExtract %v2int [[intMatTranspose]] 2
// CHECK-NEXT:         [[intVec0:%\d+]] = OpCompositeExtract %int [[intVec]] 0
// CHECK-NEXT:     [[intMatCol20:%\d+]] = OpCompositeExtract %int [[intMatCol2]] 0
// CHECK-NEXT:            [[mul5:%\d+]] = OpIMul %int [[intVec0]] [[intMatCol20]]
// CHECK-NEXT:         [[intVec1:%\d+]] = OpCompositeExtract %int [[intVec]] 1
// CHECK-NEXT:     [[intMatCol21:%\d+]] = OpCompositeExtract %int [[intMatCol2]] 1
// CHECK-NEXT:            [[mul6:%\d+]] = OpIMul %int [[intVec1]] [[intMatCol21]]
// CHECK-NEXT:              [[r2:%\d+]] = OpIAdd %int [[mul5]] [[mul6]]
// CHECK-NEXT:                 {{%\d+}} = OpCompositeConstruct %v3int [[r0]] [[r1]] [[r2]]
  int2   intVec2;
  int3 r = mul(intVec2, intMat2x3);

  // Matrix * Vector
// CHECK:        [[uintMat:%\d+]] = OpLoad %_arr_v2uint_uint_3 %uintMat3x2
// CHECK-NEXT:   [[uintVec:%\d+]] = OpLoad %v2uint %uintVec2
// CHECK-NEXT:  [[uintMat0:%\d+]] = OpCompositeExtract %v2uint [[uintMat]] 0
// CHECK-NEXT: [[uintMat00:%\d+]] = OpCompositeExtract %uint [[uintMat0]] 0
// CHECK-NEXT:  [[uintVec0:%\d+]] = OpCompositeExtract %uint [[uintVec]] 0
// CHECK-NEXT:      [[mul1:%\d+]] = OpIMul %uint [[uintMat00]] [[uintVec0]]
// CHECK-NEXT: [[uintMat01:%\d+]] = OpCompositeExtract %uint [[uintMat0]] 1
// CHECK-NEXT:  [[uintVec1:%\d+]] = OpCompositeExtract %uint [[uintVec]] 1
// CHECK-NEXT:      [[mul2:%\d+]] = OpIMul %uint [[uintMat01]] [[uintVec1]]
// CHECK-NEXT:        [[s0:%\d+]] = OpIAdd %uint [[mul1]] [[mul2]]
// CHECK-NEXT:  [[uintMat1:%\d+]] = OpCompositeExtract %v2uint [[uintMat]] 1
// CHECK-NEXT: [[uintMat10:%\d+]] = OpCompositeExtract %uint [[uintMat1]] 0
// CHECK-NEXT:  [[uintVec0:%\d+]] = OpCompositeExtract %uint [[uintVec]] 0
// CHECK-NEXT:      [[mul3:%\d+]] = OpIMul %uint [[uintMat10]] [[uintVec0]]
// CHECK-NEXT: [[uintMat11:%\d+]] = OpCompositeExtract %uint [[uintMat1]] 1
// CHECK-NEXT:  [[uintVec1:%\d+]] = OpCompositeExtract %uint [[uintVec]] 1
// CHECK-NEXT:      [[mul4:%\d+]] = OpIMul %uint [[uintMat11]] [[uintVec1]]
// CHECK-NEXT:        [[s1:%\d+]] = OpIAdd %uint [[mul3]] [[mul4]]
// CHECK-NEXT:  [[uintMat2:%\d+]] = OpCompositeExtract %v2uint [[uintMat]] 2
// CHECK-NEXT: [[uintMat20:%\d+]] = OpCompositeExtract %uint [[uintMat2]] 0
// CHECK-NEXT:  [[uintVec0:%\d+]] = OpCompositeExtract %uint [[uintVec]] 0
// CHECK-NEXT:      [[mul5:%\d+]] = OpIMul %uint [[uintMat20]] [[uintVec0]]
// CHECK-NEXT: [[uintMat21:%\d+]] = OpCompositeExtract %uint [[uintMat2]] 1
// CHECK-NEXT:  [[uintVec1:%\d+]] = OpCompositeExtract %uint [[uintVec]] 1
// CHECK-NEXT:      [[mul6:%\d+]] = OpIMul %uint [[uintMat21]] [[uintVec1]]
// CHECK-NEXT:        [[s2:%\d+]] = OpIAdd %uint [[mul5]] [[mul6]]
// CHECK-NEXT:           {{%\d+}} = OpCompositeConstruct %v3uint [[s0]] [[s1]] [[s2]]
  uint2     uintVec2;
  uint3x2   uintMat3x2;
  uint3 s = mul(uintMat3x2, uintVec2);

  // Matrix * Matrix
// CHECK:           [[lhs:%\d+]] = OpLoad %_arr_v4int_uint_2 %intMat2x4
// CHECK-NEXT:      [[rhs:%\d+]] = OpLoad %_arr_v3int_uint_4 %intMat4x3

  ///////////////////////////////////////////
  /////////// Transpose the rhs /////////////
  ///////////////////////////////////////////
// CHECK-NEXT:        [[rhs00:%\d+]] = OpCompositeExtract %int [[rhs]] 0 0
// CHECK-NEXT:        [[rhs01:%\d+]] = OpCompositeExtract %int [[rhs]] 0 1
// CHECK-NEXT:        [[rhs02:%\d+]] = OpCompositeExtract %int [[rhs]] 0 2
// CHECK-NEXT:        [[rhs10:%\d+]] = OpCompositeExtract %int [[rhs]] 1 0
// CHECK-NEXT:        [[rhs11:%\d+]] = OpCompositeExtract %int [[rhs]] 1 1
// CHECK-NEXT:        [[rhs12:%\d+]] = OpCompositeExtract %int [[rhs]] 1 2
// CHECK-NEXT:        [[rhs20:%\d+]] = OpCompositeExtract %int [[rhs]] 2 0
// CHECK-NEXT:        [[rhs21:%\d+]] = OpCompositeExtract %int [[rhs]] 2 1
// CHECK-NEXT:        [[rhs22:%\d+]] = OpCompositeExtract %int [[rhs]] 2 2
// CHECK-NEXT:        [[rhs30:%\d+]] = OpCompositeExtract %int [[rhs]] 3 0
// CHECK-NEXT:        [[rhs31:%\d+]] = OpCompositeExtract %int [[rhs]] 3 1
// CHECK-NEXT:        [[rhs32:%\d+]] = OpCompositeExtract %int [[rhs]] 3 2
// CHECK-NEXT:      [[rhsCol0:%\d+]] = OpCompositeConstruct %v4int [[rhs00]] [[rhs10]] [[rhs20]] [[rhs30]]
// CHECK-NEXT:      [[rhsCol1:%\d+]] = OpCompositeConstruct %v4int [[rhs01]] [[rhs11]] [[rhs21]] [[rhs31]]
// CHECK-NEXT:      [[rhsCol2:%\d+]] = OpCompositeConstruct %v4int [[rhs02]] [[rhs12]] [[rhs22]] [[rhs32]]
// CHECK-NEXT: [[rhsTranspose:%\d+]] = OpCompositeConstruct %_arr_v4int_uint_3 [[rhsCol0]] [[rhsCol1]] [[rhsCol2]]
  ///////////////////////////////////////////
  /////////// End: Transpose the rhs ////////
  ///////////////////////////////////////////

  ///////////////////////////////////////////
  /////////// LHS Row0 *dot* RHS Col0 ///////
  ///////////////////////////////////////////
// CHECK-NEXT:  [[lhsRow0:%\d+]] = OpCompositeExtract %v4int [[lhs]] 0
// CHECK-NEXT:  [[rhsCol0:%\d+]] = OpCompositeExtract %v4int [[rhsTranspose]] 0
// CHECK-NEXT: [[lhsRow00:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 0
// CHECK-NEXT: [[rhsCol00:%\d+]] = OpCompositeExtract %int [[rhsCol0]] 0
// CHECK-NEXT:     [[mul1:%\d+]] = OpIMul %int [[lhsRow00]] [[rhsCol00]]
// CHECK-NEXT: [[lhsRow01:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 1
// CHECK-NEXT: [[rhsCol01:%\d+]] = OpCompositeExtract %int [[rhsCol0]] 1
// CHECK-NEXT:     [[mul2:%\d+]] = OpIMul %int [[lhsRow01]] [[rhsCol01]]
// CHECK-NEXT: [[lhsRow02:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 2
// CHECK-NEXT: [[rhsCol02:%\d+]] = OpCompositeExtract %int [[rhsCol0]] 2
// CHECK-NEXT:     [[mul3:%\d+]] = OpIMul %int [[lhsRow02]] [[rhsCol02]]
// CHECK-NEXT: [[lhsRow03:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 3
// CHECK-NEXT: [[rhsCol03:%\d+]] = OpCompositeExtract %int [[rhsCol0]] 3
// CHECK-NEXT:     [[mul4:%\d+]] = OpIMul %int [[lhsRow03]] [[rhsCol03]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul1]] [[mul2]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul]] [[mul3]]
// CHECK-NEXT:      [[t00:%\d+]] = OpIAdd %int [[mul]] [[mul4]]
  ///////////////////////////////////////////
  ////// END: LHS Row0 *dot* RHS Col0 ///////
  ///////////////////////////////////////////

  ///////////////////////////////////////////
  /////////// LHS Row0 *dot* RHS Col1 ///////
  ///////////////////////////////////////////
// CHECK-NEXT:  [[rhsCol1:%\d+]] = OpCompositeExtract %v4int [[rhsTranspose]] 1
// CHECK-NEXT: [[lhsRow00:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 0
// CHECK-NEXT: [[rhsCol10:%\d+]] = OpCompositeExtract %int [[rhsCol1]] 0
// CHECK-NEXT:     [[mul5:%\d+]] = OpIMul %int [[lhsRow00]] [[rhsCol10]]
// CHECK-NEXT: [[lhsRow01:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 1
// CHECK-NEXT: [[rhsCol11:%\d+]] = OpCompositeExtract %int [[rhsCol1]] 1
// CHECK-NEXT:     [[mul6:%\d+]] = OpIMul %int [[lhsRow01]] [[rhsCol11]]
// CHECK-NEXT: [[lhsRow02:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 2
// CHECK-NEXT: [[rhsCol12:%\d+]] = OpCompositeExtract %int [[rhsCol1]] 2
// CHECK-NEXT:     [[mul7:%\d+]] = OpIMul %int [[lhsRow02]] [[rhsCol12]]
// CHECK-NEXT: [[lhsRow03:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 3
// CHECK-NEXT: [[rhsCol13:%\d+]] = OpCompositeExtract %int [[rhsCol1]] 3
// CHECK-NEXT:     [[mul8:%\d+]] = OpIMul %int [[lhsRow03]] [[rhsCol13]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul5]] [[mul6]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul]] [[mul7]]
// CHECK-NEXT:      [[t01:%\d+]] = OpIAdd %int [[mul]] [[mul8]]
  ///////////////////////////////////////////
  ////// END: LHS Row0 *dot* RHS Col1 ///////
  ///////////////////////////////////////////

  ///////////////////////////////////////////
  /////////// LHS Row0 *dot* RHS Col2 ///////
  ///////////////////////////////////////////
// CHECK-NEXT:  [[rhsCol2:%\d+]] = OpCompositeExtract %v4int [[rhsTranspose]] 2
// CHECK-NEXT: [[lhsRow00:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 0
// CHECK-NEXT: [[rhsCol20:%\d+]] = OpCompositeExtract %int [[rhsCol2]] 0
// CHECK-NEXT:     [[mul9:%\d+]] = OpIMul %int [[lhsRow00]] [[rhsCol20]]
// CHECK-NEXT: [[lhsRow01:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 1
// CHECK-NEXT: [[rhsCol21:%\d+]] = OpCompositeExtract %int [[rhsCol2]] 1
// CHECK-NEXT:    [[mul10:%\d+]] = OpIMul %int [[lhsRow01]] [[rhsCol21]]
// CHECK-NEXT: [[lhsRow02:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 2
// CHECK-NEXT: [[rhsCol22:%\d+]] = OpCompositeExtract %int [[rhsCol2]] 2
// CHECK-NEXT:    [[mul11:%\d+]] = OpIMul %int [[lhsRow02]] [[rhsCol22]]
// CHECK-NEXT: [[lhsRow03:%\d+]] = OpCompositeExtract %int [[lhsRow0]] 3
// CHECK-NEXT: [[rhsCol23:%\d+]] = OpCompositeExtract %int [[rhsCol2]] 3
// CHECK-NEXT:    [[mul12:%\d+]] = OpIMul %int [[lhsRow03]] [[rhsCol23]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul9]] [[mul10]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul]] [[mul11]]
// CHECK-NEXT:      [[t02:%\d+]] = OpIAdd %int [[mul]] [[mul12]]
  ///////////////////////////////////////////
  ////// END: LHS Row0 *dot* RHS Col2 ///////
  ///////////////////////////////////////////

// Result row 0:
// CHECK-NEXT: [[t0:%\d+]] = OpCompositeConstruct %v3int [[t00]] [[t01]] [[t02]]

  ///////////////////////////////////////////
  /////////// LHS Row1 *dot* RHS Col0 ///////
  ///////////////////////////////////////////
// CHECK-NEXT:  [[lhsRow1:%\d+]] = OpCompositeExtract %v4int [[lhs]] 1
// CHECK-NEXT:  [[rhsCol0:%\d+]] = OpCompositeExtract %v4int [[rhsTranspose]] 0
// CHECK-NEXT: [[lhsRow10:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 0
// CHECK-NEXT: [[rhsCol00:%\d+]] = OpCompositeExtract %int [[rhsCol0]] 0
// CHECK-NEXT:     [[mul1:%\d+]] = OpIMul %int [[lhsRow10]] [[rhsCol00]]
// CHECK-NEXT: [[lhsRow11:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 1
// CHECK-NEXT: [[rhsCol01:%\d+]] = OpCompositeExtract %int [[rhsCol0]] 1
// CHECK-NEXT:     [[mul2:%\d+]] = OpIMul %int [[lhsRow11]] [[rhsCol01]]
// CHECK-NEXT: [[lhsRow12:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 2
// CHECK-NEXT: [[rhsCol02:%\d+]] = OpCompositeExtract %int [[rhsCol0]] 2
// CHECK-NEXT:     [[mul3:%\d+]] = OpIMul %int [[lhsRow12]] [[rhsCol02]]
// CHECK-NEXT: [[lhsRow13:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 3
// CHECK-NEXT: [[rhsCol03:%\d+]] = OpCompositeExtract %int [[rhsCol0]] 3
// CHECK-NEXT:     [[mul4:%\d+]] = OpIMul %int [[lhsRow13]] [[rhsCol03]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul1]] [[mul2]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul]] [[mul3]]
// CHECK-NEXT:      [[t10:%\d+]] = OpIAdd %int [[mul]] [[mul4]]
  ///////////////////////////////////////////
  ////// END: LHS Row1 *dot* RHS Col0 ///////
  ///////////////////////////////////////////

  ///////////////////////////////////////////
  /////////// LHS Row1 *dot* RHS Col1 ///////
  ///////////////////////////////////////////
// CHECK-NEXT:  [[rhsCol1:%\d+]] = OpCompositeExtract %v4int [[rhsTranspose]] 1
// CHECK-NEXT: [[lhsRow10:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 0
// CHECK-NEXT: [[rhsCol10:%\d+]] = OpCompositeExtract %int [[rhsCol1]] 0
// CHECK-NEXT:     [[mul5:%\d+]] = OpIMul %int [[lhsRow10]] [[rhsCol10]]
// CHECK-NEXT: [[lhsRow11:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 1
// CHECK-NEXT: [[rhsCol11:%\d+]] = OpCompositeExtract %int [[rhsCol1]] 1
// CHECK-NEXT:     [[mul6:%\d+]] = OpIMul %int [[lhsRow11]] [[rhsCol11]]
// CHECK-NEXT: [[lhsRow12:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 2
// CHECK-NEXT: [[rhsCol12:%\d+]] = OpCompositeExtract %int [[rhsCol1]] 2
// CHECK-NEXT:     [[mul7:%\d+]] = OpIMul %int [[lhsRow12]] [[rhsCol12]]
// CHECK-NEXT: [[lhsRow13:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 3
// CHECK-NEXT: [[rhsCol13:%\d+]] = OpCompositeExtract %int [[rhsCol1]] 3
// CHECK-NEXT:     [[mul8:%\d+]] = OpIMul %int [[lhsRow13]] [[rhsCol13]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul5]] [[mul6]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul]] [[mul7]]
// CHECK-NEXT:      [[t11:%\d+]] = OpIAdd %int [[mul]] [[mul8]]
  ///////////////////////////////////////////
  ////// END: LHS Row1 *dot* RHS Col1 ///////
  ///////////////////////////////////////////

  ///////////////////////////////////////////
  /////////// LHS Row1 *dot* RHS Col2 ///////
  ///////////////////////////////////////////
// CHECK-NEXT:  [[rhsCol2:%\d+]] = OpCompositeExtract %v4int [[rhsTranspose]] 2
// CHECK-NEXT: [[lhsRow10:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 0
// CHECK-NEXT: [[rhsCol20:%\d+]] = OpCompositeExtract %int [[rhsCol2]] 0
// CHECK-NEXT:     [[mul9:%\d+]] = OpIMul %int [[lhsRow10]] [[rhsCol20]]
// CHECK-NEXT: [[lhsRow11:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 1
// CHECK-NEXT: [[rhsCol21:%\d+]] = OpCompositeExtract %int [[rhsCol2]] 1
// CHECK-NEXT:    [[mul10:%\d+]] = OpIMul %int [[lhsRow11]] [[rhsCol21]]
// CHECK-NEXT: [[lhsRow12:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 2
// CHECK-NEXT: [[rhsCol22:%\d+]] = OpCompositeExtract %int [[rhsCol2]] 2
// CHECK-NEXT:    [[mul11:%\d+]] = OpIMul %int [[lhsRow12]] [[rhsCol22]]
// CHECK-NEXT: [[lhsRow13:%\d+]] = OpCompositeExtract %int [[lhsRow1]] 3
// CHECK-NEXT: [[rhsCol23:%\d+]] = OpCompositeExtract %int [[rhsCol2]] 3
// CHECK-NEXT:    [[mul12:%\d+]] = OpIMul %int [[lhsRow13]] [[rhsCol23]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul9]] [[mul10]]
// CHECK-NEXT:      [[mul:%\d+]] = OpIAdd %int [[mul]] [[mul11]]
// CHECK-NEXT:      [[t12:%\d+]] = OpIAdd %int [[mul]] [[mul12]]
  ///////////////////////////////////////////
  ////// END: LHS Row1 *dot* RHS Col2 ///////
  ///////////////////////////////////////////

// Result row 1:
// CHECK-NEXT: [[t1:%\d+]] = OpCompositeConstruct %v3int [[t10]] [[t11]] [[t12]]

// Final result:
// CHECK-NEXT:    {{%\d+}} = OpCompositeConstruct %_arr_v3int_uint_2 [[t0]] [[t1]]
  int4x3 intMat4x3;
  int2x3 t = mul(intMat2x4, intMat4x3);


//
// 1-D matrices passed to mul
//

// mul( Mat(1xM) * Mat(MxN) ) --> Mat(1xN) vector
// mul( Mat(1xM) * Mat(Mx1) ) --> Scalar
// mul( Mat(Mx1) * Mat(1xN) ) --> Mat(MxN) matrix
  float1x3 mat1x3;
  float3x2 mat3x2;
  float3x1 mat3x1;
  float1x4 mat1x4;

// CHECK:       [[mat1x3:%\d+]] = OpLoad %v3float %mat1x3
// CHECK-NEXT:  [[mat3x2:%\d+]] = OpLoad %mat3v2float %mat3x2
// CHECK-NEXT: [[result1:%\d+]] = OpMatrixTimesVector %v2float [[mat3x2]] [[mat1x3]]
// CHECK-NEXT:                    OpStore %result1 [[result1]]
  float1x2   result1 = mul( mat1x3, mat3x2 ); // result is float2 vector

// CHECK:       [[mat1x3:%\d+]] = OpLoad %v3float %mat1x3
// CHECK-NEXT:  [[mat3x1:%\d+]] = OpLoad %v3float %mat3x1
// CHECK-NEXT: [[result2:%\d+]] = OpDot %float [[mat1x3]] [[mat3x1]]
// CHECK-NEXT:                    OpStore %result2 [[result2]]
  float      result2 = mul( mat1x3, mat3x1 ); // result is scalar

// CHECK:       [[mat3x1:%\d+]] = OpLoad %v3float %mat3x1
// CHECK-NEXT:  [[mat1x4:%\d+]] = OpLoad %v4float %mat1x4
// CHECK-NEXT:   [[elem0:%\d+]] = OpCompositeExtract %float [[mat3x1]] 0
// CHECK-NEXT:    [[row0:%\d+]] = OpVectorTimesScalar %v4float [[mat1x4]] [[elem0]]
// CHECK-NEXT:   [[elem1:%\d+]] = OpCompositeExtract %float [[mat3x1]] 1
// CHECK-NEXT:    [[row1:%\d+]] = OpVectorTimesScalar %v4float [[mat1x4]] [[elem1]]
// CHECK-NEXT:   [[elem2:%\d+]] = OpCompositeExtract %float [[mat3x1]] 2
// CHECK-NEXT:    [[row2:%\d+]] = OpVectorTimesScalar %v4float [[mat1x4]] [[elem2]]
// CHECK-NEXT: [[result3:%\d+]] = OpCompositeConstruct %mat3v4float [[row0]] [[row1]] [[row2]]
// CHECK-NEXT:                    OpStore %result3 [[result3]]
  float3x4   result3 = mul( mat3x1, mat1x4 ); // result is float3x4 matrix
}
