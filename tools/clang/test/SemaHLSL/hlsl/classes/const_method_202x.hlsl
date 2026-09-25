// RUN: %dxc -T ps_6_0 -E main -HV 202x -ast-dump %s | FileCheck %s

// Verify the parser accepts `const` instance methods in HLSL 202x and that
// overload resolution selects the const overload for const objects and the
// non-const overload for non-const objects, including for out-of-line
// definitions and class templates.

struct S {
  int x;
  int get() { return 100; }
  int get() const { return 200; }
  int outOfLine() const;
};

// Two distinct overloads: one const-qualified, one not.
// CHECK: CXXMethodDecl [[NC:0x[0-9a-f]+]] {{.*}} used get 'int ()'
// CHECK: CXXMethodDecl [[C:0x[0-9a-f]+]] {{.*}} used get 'int () const'
// CHECK: CXXMethodDecl [[OOLDecl:0x[0-9a-f]+]] {{.*}} outOfLine 'int () const'

int S::outOfLine() const { return x; }
// CHECK: CXXMethodDecl {{0x[0-9a-f]+}} parent {{0x[0-9a-f]+}} prev [[OOLDecl]] {{.*}} used outOfLine 'int () const'

template <typename T> struct W {
  T v;
  T get() { return v; }
  T get() const { return v; }
};

// CHECK: ClassTemplateSpecializationDecl {{.*}} struct W definition
// CHECK: CXXMethodDecl [[WNC:0x[0-9a-f]+]] {{.*}} used get 'int ()'
// CHECK: CXXMethodDecl [[WC:0x[0-9a-f]+]] {{.*}} used get 'int () const'

cbuffer CB {
  S cs; // cs is const because it lives in a cbuffer.
};

float4 main() : SV_Target {
  S s = {1};
  int a = s.get();   // expect non-const overload
  int b = cs.get();  // expect const overload
  int c = cs.outOfLine();
  W<int> w = {2};
  const W<int> cw = {3};
  int d = w.get();   // expect non-const overload
  int e = cw.get();  // expect const overload
  return float4(a, b, c, d + e);
}

// CHECK: MemberExpr {{.*}} .get [[NC]]
// CHECK-NEXT: DeclRefExpr {{.*}} 'S' lvalue Var {{0x[0-9a-f]+}} 's' 'S'
// CHECK: MemberExpr {{.*}} .get [[C]]
// CHECK-NEXT: DeclRefExpr {{.*}} 'const S' lvalue Var {{0x[0-9a-f]+}} 'cs' 'const S'
// CHECK: MemberExpr {{.*}} .outOfLine
// CHECK-NEXT: DeclRefExpr {{.*}} 'const S' lvalue Var {{0x[0-9a-f]+}} 'cs' 'const S'
// CHECK: MemberExpr {{.*}} .get [[WNC]]
// CHECK-NEXT: DeclRefExpr {{.*}} 'W<int>':'W<int>' lvalue Var {{0x[0-9a-f]+}} 'w'
// CHECK: MemberExpr {{.*}} .get [[WC]]
// CHECK-NEXT: DeclRefExpr {{.*}} 'const W<int>':'const W<int>' lvalue Var {{0x[0-9a-f]+}} 'cw'
