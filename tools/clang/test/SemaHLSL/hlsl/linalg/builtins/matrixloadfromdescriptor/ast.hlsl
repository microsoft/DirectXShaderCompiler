// REQUIRES: dxil-1-10
// RUN: %dxc -I %hlsl_headers -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_MatrixLoadFromDescriptor 'void (__builtin_LinAlgMatrix & {{.*}}, RWByteAddressBuffer, unsigned int, unsigned int, unsigned int)' extern
// CHECK-NEXT: ParmVarDecl {{.*}} ret '__builtin_LinAlgMatrix &&__restrict {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} buf 'RWByteAddressBuffer'
// CHECK-NEXT: ParmVarDecl {{.*}} offset 'unsigned int'
// CHECK-NEXT: ParmVarDecl {{.*}} stride 'unsigned int'
// CHECK-NEXT: ParmVarDecl {{.*}} layout 'unsigned int'
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 410
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""

RWByteAddressBuffer inbuf;

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::I32, 5, 4, MatrixUse::B, MatrixScope::ThreadGroup)]] mat;
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat, inbuf, 0, 0, 0);
}
