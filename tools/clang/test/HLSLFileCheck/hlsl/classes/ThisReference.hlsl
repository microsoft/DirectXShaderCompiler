// RUN: %dxc -T cs_6_6 -E main -ast-dump %s | FileCheck %s

struct pair {
  int First;
  float Second;

  int first() {
    return this.First;
  }

  float second() {
    return Second;
  }
};

[numthreads(1,1,1)]
void main() {
  pair Vals = {1, 2.0};
  Vals.First = Vals.first();
  Vals.Second = Vals.second();
}

// CHECK:      CXXMethodDecl 0x{{[0-9a-zA-Z]+}} <line:7:3, line:9:3> line:7:7 used first 'int ()'
// CHECK-NEXT: `-CompoundStmt 0x{{[0-9a-zA-Z]+}} <col:15, line:9:3>
// CHECK-NEXT: `-ReturnStmt 0x{{[0-9a-zA-Z]+}} <line:8:5, col:17>
// CHECK-NEXT: `-ImplicitCastExpr 0x{{[0-9a-zA-Z]+}} <col:12, col:17> 'int' <LValueToRValue>
// CHECK-NEXT: `-MemberExpr 0x{{[0-9a-zA-Z]+}} <col:12, col:17> 'int' lvalue .First 0x{{[0-9a-zA-Z]+}}
// CHECK-NEXT: `-CXXThisExpr 0x{{[0-9a-zA-Z]+}} <col:12> 'pair' lvalue this
// CHECK-NEXT: `-CXXMethodDecl 0x{{[0-9a-zA-Z]+}} <line:11:3, line:13:3> line:11:9 used second 'float ()'
// CHECK-NEXT: `-CompoundStmt 0x{{[0-9a-zA-Z]+}} <col:18, line:13:3>
// CHECK-NEXT: `-ReturnStmt 0x{{[0-9a-zA-Z]+}} <line:12:5, col:12>
// CHECK-NEXT: `-ImplicitCastExpr 0x{{[0-9a-zA-Z]+}} <col:12> 'float' <LValueToRValue>
// CHECK-NEXT: `-MemberExpr 0x{{[0-9a-zA-Z]+}} <col:12> 'float' lvalue .Second 0x{{[0-9a-zA-Z]+}}
// CHECK-NEXT: `-CXXThisExpr 0x{{[0-9a-zA-Z]+}} <col:12> 'pair' lvalue this
