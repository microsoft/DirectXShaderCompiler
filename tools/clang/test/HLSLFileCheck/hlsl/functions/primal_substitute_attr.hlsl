// RUN: %dxc -T ps_6_0 -E main %s | FileCheck %s -check-prefix=DXIL
// RUN: %dxc -T ps_6_0 -E main -ast-dump %s | FileCheck %s -check-prefix=AST

// DXIL: target triple
// AST: FunctionDecl {{.*}} reference 'float (float)'
// AST: HLSLPrimalSubstituteOfAttr

float original(float x) { return x + 1.0f; }

[[dxc::PrimalSubstituteOf(original)]]
[[dxc::AutoDiff(fwd, bwd)]]
float reference(float x) { return x * x; }

float main(float x : A) : SV_Target { return original(x); }
