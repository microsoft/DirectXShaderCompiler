// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 %s | FileCheck %s

// This test is using 2 LinAlgMatrix operations:
// - __builtin_LinAlg_FillMatrix - has the matrix as a return value
// - __builtin_LinAlg_MatrixLength - has the matrix as an argument
// This is done to verify that target types are correctly collected from both
// return values and arguments of LinAlgMatrix operations.

uint useMatrix1() {
  // Matrix<ComponentType::I32, 4, 5, MatrixUse::A, MatrixScope::Thread> m;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 4, 5, 0, 0)]] mat1;
  // mat1 = Matrix::Splat(5);
  __builtin_LinAlg_FillMatrix(mat1, 5);

  // Matrix<ComponentType::I32, 4, 5, MatrixUse::A, MatrixScope::Thread> m;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 3, 3, 0, 0)]] mat2;
  // mat2 = Matrix::Splat(5);
  return __builtin_LinAlg_MatrixLength(mat2);
}

uint useMatrix2() {
  // Matrix<ComponentType::F64, 2, 2, MatrixUse::B, MatrixScope::Wave> m;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(10, 2, 2, 1, 1)]] mat3;
  // mat3 = Matrix::Splat(5);
  __builtin_LinAlg_FillMatrix(mat3, 5);
  return __builtin_LinAlg_MatrixLength(mat3);
}

RWBuffer<uint> Out;

[numthreads(4,1,1)]
void main() {
  Out[0] = useMatrix1();
}

// CHECK: !dx.targetTypes = !{!{{[0-9]+}}, !{{[0-9]+}}}
// CHECK: !{{[0-9]+}} = !{%dx.types.LinAlgMatrixC4M4N5U0S0 undef, i32 4, i32 4, i32 5, i32 0, i32 0}
// CHECK: !{{[0-9]+}} = !{%dx.types.LinAlgMatrixC5M3N3U0S0 undef, i32 5, i32 3, i32 3, i32 0, i32 0}
// CHECK-NOT: !{%dx.types.LinAlgMatrixC10M2N2U1S1 undef, i32 10, i32 2, i32 2, i32 1, i32 1}
