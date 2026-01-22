// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -verify %s

__builtin_LinAlg_Matrix global_handle;

static __builtin_LinAlg_Matrix static_handle;

groupshared __builtin_LinAlg_Matrix gs_handle;

__builtin_LinAlg_Matrix array[2];

cbuffer CB {
  __builtin_LinAlg_Matrix cb_handle;
};

struct S {
  __builtin_LinAlg_Matrix handle;
};

S s;

void f1(__builtin_LinAlg_Matrix m);

__builtin_LinAlg_Matrix f2();

// expected-error@+1 {{typedef redefinition with different types ('int' vs '__builtin_LinAlg Matrix')}}
typedef int __builtin_LinAlg_Matrix;

[shader("compute")]
[numthreads(4,1,1)]
void main() {
  __builtin_LinAlg_Matrix m;

  m = s.handle;

  f1(m);
  m = f2();

  // expected-error@+1 {{numeric type expected}}
  m++;
  
  // expected-error@+1 {{numeric type expected}}
  m = m * 2;

  // expected-error@+1 {{numeric type expected}}
  bool a = m && s.handle;
}
