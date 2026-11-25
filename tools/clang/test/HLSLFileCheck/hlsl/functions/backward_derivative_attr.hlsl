// RUN: %dxc -T ps_6_0 -E main %s | FileCheck %s -check-prefix=DXIL
// RUN: %dxc -T ps_6_0 -E main -ast-dump %s | FileCheck %s -check-prefix=AST

// DXIL: target triple
// AST: FunctionDecl {{.*}} square 'float (float)'
// AST: HLSLBackwardDerivativeAttr

void bwd_square(float x, float dResult, out float dX) {
  dX = 2.0f * x * dResult;
}

[[dxc::BackwardDerivative(bwd_square)]]
float square(float x) { return x * x; }

float main(float x : A) : SV_Target { return square(x); }
