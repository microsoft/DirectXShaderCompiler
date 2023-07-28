// RUN: %dxc -T lib_6_6 %s -HV 2018 -ast-dump | FileCheck %s -check-prefix=AST
// RUN: %dxc -T lib_6_6 %s -HV 2018 -fcgl | FileCheck %s
// RUN: %dxc -T lib_6_6 %s -HV 2021 -ast-dump | FileCheck %s -check-prefix=AST
// RUN: %dxc -T lib_6_6 %s -HV 2021 -fcgl | FileCheck %s

// AST: CXXRecordDecl {{.*}} referenced class array definition
// AST-NEXT: CXXRecordDecl {{.*}} implicit class array
// AST-NEXT: FieldDecl {{.*}} referenced mArr 'float [4]'
// AST-NEXT: CXXRecordDecl {{.*}} class array_ext definition
// AST-NEXT: public 'array'
// AST-NEXT: CXXRecordDecl {{.*}} implicit class array_ext
// AST-NEXT: CXXMethodDecl {{.*}} test 'float ()'
// AST-NEXT: CompoundStmt
// AST-NEXT: ReturnStmt
// AST-NEXT: ImplicitCastExpr {{.*}} 'float' <LValueToRValue>
// AST-NEXT: ArraySubscriptExpr {{.*}} 'float' lvalue
// AST-NEXT: ImplicitCastExpr {{.*}} 'float [4]' <LValueToRValue>
// AST-NEXT: MemberExpr {{.*}} 'float [4]' lvalue .mArr
// AST-NEXT: ImplicitCastExpr {{.*}} 'array' lvalue <UncheckedDerivedToBase (array)>
// AST-NEXT: CXXThisExpr {{.*}} 'array_ext' lvalue this
// AST-NEXT: IntegerLiteral {{.*}} 'literal int' 0

class array {
  float mArr[4];
};

class array_ext : array {
  float test() { return array::mArr[0]; }
};

// CHECK: define linkonce_odr float @"\01?test@array_ext@{{.*}}"(%class.array_ext* [[this:%.+]])
// CHECK: [[basePtr:%[0-9]+]] = bitcast %class.array_ext* [[this]] to %class.array*
// CHECK: [[mArr:%[0-9]+]] = getelementptr inbounds %class.array, %class.array* [[basePtr]], i32 0, i32 0
// CHECK: [[elemPtr:%[0-9]+]] = getelementptr inbounds [4 x float], [4 x float]* [[mArr]], i32 0, i32 0
// CHECK: [[Val:%[0-9]+]] = load float, float* [[elemPtr]]
// CHECK: ret float [[Val]]

// This function only exists to force generation of the internal methods
float fn() {
  array_ext arr1;
  return arr1.test();
}
