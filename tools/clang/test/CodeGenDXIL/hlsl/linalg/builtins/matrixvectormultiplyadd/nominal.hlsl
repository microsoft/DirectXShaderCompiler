// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -E main -fcgl %s | FileCheck %s --check-prefix=CHECK2

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 3, 4, 0, 0)]] mat1;
  float4 vec = {1,2,3,4};
  float4 result;

  // CHECK: call <4 x float> @dx.op.linAlgMatVecMulAdd.v4f32.mC5M3N4U0S0.v4f32.v4f32(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC5M3N4U0S0 {{.*}}, i1 true, <4 x float> <float 1.000000e+00,
  // CHECK-SAME: float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, i32 1, <4 x float> {{.*}}, i32 0)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)

  // CHECK2: call void @"dx.hl.op..void (i32, <4 x float>*, %dx.types.LinAlgMatrixC5M3N4U0S0, i1, <4 x float>,
  // CHECK2-SAME: i32, <4 x float>, i32)"(i32 419, <4 x float>* %result, %dx.types.LinAlgMatrixC5M3N4U0S0 %{{[0-9]+}},
  // CHECK2-SAME: i1 true, <4 x float> %{{[0-9]+}}, i32 1, <4 x float> %{{[0-9]+}}, i32 0)

  __builtin_LinAlg_MatrixVectorMultiplyAdd(result, mat1, true, vec, 1, result, 0);

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 3, 4, 0, 0)]] mat2;
  double4 vec2 = {1,2,3,4};
  double4 result2;

  // CHECK: call <4 x double> @dx.op.linAlgMatVecMulAdd.v4f64.mC5M3N4U0S0.v4f64.v4f64(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC5M3N4U0S0 {{.*}}, i1 true, <4 x double> <double 1.000000e+00,
  // CHECK-SAME: double 2.000000e+00, double 3.000000e+00, double 4.000000e+00>, i32 1, <4 x double> {{.*}}, i32 0)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)

  // CHECK2: call void @"dx.hl.op..void (i32, <4 x double>*, %dx.types.LinAlgMatrixC5M3N4U0S0, i1, <4 x double>,
  // CHECK2-SAME: i32, <4 x double>, i32)"(i32 419, <4 x double>* %result2, %dx.types.LinAlgMatrixC5M3N4U0S0 %{{[0-9]+}},
  // CHECK2-SAME: i1 true, <4 x double> %{{[0-9]+}}, i32 1, <4 x double> %{{[0-9]+}}, i32 0)

  __builtin_LinAlg_MatrixVectorMultiplyAdd(result2, mat2, true, vec2, 1, result2, 0);

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 3, 4, 0, 0)]] mat3;
  vector<int64_t, 4> vec3 = {1,2,3,4};
  vector<int64_t, 4> result3;

  // CHECK: call <4 x i64> @dx.op.linAlgMatVecMulAdd.v4i64.mC5M3N4U0S0.v4i64.v4i64(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC5M3N4U0S0 {{.*}}, i1 true, <4 x i64> <i64 1,
  // CHECK-SAME: i64 2, i64 3, i64 4>, i32 1, <4 x i64> {{.*}}, i32 0)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)

  // CHECK2: call void @"dx.hl.op..void (i32, <4 x i64>*, %dx.types.LinAlgMatrixC5M3N4U0S0, i1, <4 x i64>,
  // CHECK2-SAME: i32, <4 x i64>, i32)"(i32 419, <4 x i64>* %result3, %dx.types.LinAlgMatrixC5M3N4U0S0 %{{[0-9]+}},
  // CHECK2-SAME: i1 true, <4 x i64> %{{[0-9]+}}, i32 1, <4 x i64> %{{[0-9]+}}, i32 0)

  __builtin_LinAlg_MatrixVectorMultiplyAdd(result3, mat3, true, vec3, 1, result3, 0);
}
