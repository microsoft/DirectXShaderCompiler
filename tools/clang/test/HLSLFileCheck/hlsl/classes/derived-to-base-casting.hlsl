// RUN: %dxc -T lib_6_4 -ast-dump %s | FileCheck %s


struct Base {
  int b;
};

struct Derived : Base {
  int d;
};

struct DerivedAgain : Derived {
  int a;
};

void DerivedToBase() {
  DerivedAgain da1, da2;

  (Base)da1 = (Base)da2;
  (Derived)da1 = (Derived)da2;
  (DerivedAgain)da1 = (DerivedAgain)da2;
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <line:16:1, line:22:1> line:16:6 DerivedToBase 'void ()'
// CHECK-NEXT: CompoundStmt {{0x[0-9a-fA-F]+}} <col:22, line:22:1>
// CHECK-NEXT: DeclStmt {{0x[0-9a-fA-F]+}} <line:17:3, col:24>
// CHECK-NEXT: VarDecl {{0x[0-9a-fA-F]+}} <col:3, col:16> col:16 used da1 'DerivedAgain'
// CHECK-NEXT: VarDecl {{0x[0-9a-fA-F]+}} <col:3, col:21> col:21 used da2 'DerivedAgain'
// CHECK-NEXT: BinaryOperator {{0x[0-9a-fA-F]+}} <line:19:3, col:21> 'Base' '='
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:3, col:9> 'Base' lvalue <HLSLDerivedToBase (Derived -> Base)>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:9> 'DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'da1' 'DerivedAgain'
// CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-fA-F]+}} <col:15, col:21> 'Base' <LValueToRValue>
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:15, col:21> 'Base' lvalue <HLSLDerivedToBase (Derived -> Base)>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:21> 'DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'da2' 'DerivedAgain'
// CHECK-NEXT: BinaryOperator {{0x[0-9a-fA-F]+}} <line:20:3, col:27> 'Derived' '='
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:3, col:12> 'Derived' lvalue <HLSLDerivedToBase (Derived)>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:12> 'DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'da1' 'DerivedAgain'
// CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-fA-F]+}} <col:18, col:27> 'Derived' <LValueToRValue>
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:18, col:27> 'Derived' lvalue <HLSLDerivedToBase (Derived)>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:27> 'DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'da2' 'DerivedAgain'
// CHECK-NEXT: BinaryOperator {{0x[0-9a-fA-F]+}} <line:21:3, col:37> 'DerivedAgain' '='
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:3, col:17> 'DerivedAgain' lvalue <NoOp>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:17> 'DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'da1' 'DerivedAgain'
// CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-fA-F]+}} <col:23, col:37> 'DerivedAgain' <LValueToRValue>
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:23, col:37> 'DerivedAgain' lvalue <NoOp>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:37> 'DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'da2' 'DerivedAgain'

groupshared DerivedAgain daGS;

// CHECK-NEXT: VarDecl {{0x[0-9a-fA-F]+}} <line:48:1, col:26> col:26 used daGS '__attribute__((address_space(3))) DerivedAgain'
// CHECK-NEXT: HLSLGroupSharedAttr {{0x[0-9a-fA-F]+}} <col:1>

void DerivedToBaseGS() {
  DerivedAgain da1;

  (Base)da1 = (Base)daGS;
  (Derived)da1 = (Derived)daGS;
  (DerivedAgain)da1 = (DerivedAgain)daGS;
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <line:53:1, line:59:1> line:53:6 DerivedToBaseGS 'void ()'
// CHECK-NEXT: CompoundStmt {{0x[0-9a-fA-F]+}} <col:24, line:59:1>
// CHECK-NEXT: DeclStmt {{0x[0-9a-fA-F]+}} <line:54:3, col:19>
// CHECK-NEXT: VarDecl {{0x[0-9a-fA-F]+}} <col:3, col:16> col:16 used da1 'DerivedAgain'
// CHECK-NEXT: BinaryOperator {{0x[0-9a-fA-F]+}} <line:56:3, col:21> 'Base' '='
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:3, col:9> 'Base' lvalue <HLSLDerivedToBase (Derived -> Base)>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:9> 'DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'da1' 'DerivedAgain'
// CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-fA-F]+}} <col:15, col:21> 'Base' <LValueToRValue>
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:15, col:21> '__attribute__((address_space(3))) Base' lvalue <HLSLDerivedToBase (Derived -> Base)>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:21> '__attribute__((address_space(3))) DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'daGS' '__attribute__((address_space(3))) DerivedAgain'
// CHECK-NEXT: BinaryOperator {{0x[0-9a-fA-F]+}} <line:57:3, col:27> 'Derived' '='
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:3, col:12> 'Derived' lvalue <HLSLDerivedToBase (Derived)>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:12> 'DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'da1' 'DerivedAgain'
// CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-fA-F]+}} <col:18, col:27> 'Derived' <LValueToRValue>
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:18, col:27> '__attribute__((address_space(3))) Derived' lvalue <HLSLDerivedToBase (Derived)>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:27> '__attribute__((address_space(3))) DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'daGS' '__attribute__((address_space(3))) DerivedAgain'
// CHECK-NEXT: BinaryOperator {{0x[0-9a-fA-F]+}} <line:58:3, col:37> 'DerivedAgain' '='
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:3, col:17> 'DerivedAgain' lvalue <NoOp>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:17> 'DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'da1' 'DerivedAgain'
// CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-fA-F]+}} <col:23, col:37> 'DerivedAgain' <LValueToRValue>
// CHECK-NEXT: CStyleCastExpr {{0x[0-9a-fA-F]+}} <col:23, col:37> 'DerivedAgain' lvalue <NoOp>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-fA-F]+}} <col:37> '__attribute__((address_space(3))) DerivedAgain' lvalue Var {{0x[0-9a-fA-F]+}} 'daGS' '__attribute__((address_space(3))) DerivedAgain'
