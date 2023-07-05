// RUN: %dxc -T lib_6_3 -HV 2021 -fcgl %s | FileCheck %s
// RUN: %dxc -T lib_6_3 -HV 2021 -ast-dump %s | FileCheck %s -check-prefix=AST

// Make sure dependent size matrix works.

// CHECK-DAG:%"class.d_mat2<float, 3>" = type { %"class.d_mat<float, 3, 2>" }
// CHECK-DAG:%"class.d_mat<float, 3, 2>" = type { %class.matrix.float.3.2 }
// CHECK-DAG:%class.matrix.float.3.2 = type { [3 x <2 x float>] }
// CHECK-DAG:%"$Globals" = type { %"class.d_mat2<float, 3>" }
// CHECK-DAG:%"class.d_mat<float, 2, 2>" = type { %class.matrix.float.2.2 }
// CHECK-DAG:%class.matrix.float.2.2 = type { [2 x <2 x float>] }


// AST: ClassTemplateDecl 0x{{.+}} d_mat
// AST-NEXT: TemplateTypeParmDecl 0x{{.+}} referenced typename T
// AST-NEXT: NonTypeTemplateParmDecl 0x{{.+}} referenced 'int' r
// AST-NEXT: NonTypeTemplateParmDecl 0x{{.+}} referenced 'int' c
// AST-NEXT: CXXRecordDecl 0x{{.+}} class d_mat definition
// AST-NEXT: CXXRecordDecl 0x{{.+}} implicit class d_mat
// AST-NEXT: FieldDecl 0x{{.+}} m 'matrix<T, r, c>'
// AST-NEXT: ClassTemplateSpecializationDecl 0x{{.+}} class d_mat definition
// AST-NEXT: TemplateArgument type 'float'
// AST-NEXT: TemplateArgument integral 2
// AST-NEXT: TemplateArgument integral 2
// AST-NEXT: CXXRecordDecl 0x{{.+}} implicit class d_mat
// AST-NEXT: FieldDecl 0x{{.+}} referenced m 'matrix<float, 2, 2>'
// AST-NEXT: ClassTemplateSpecializationDecl 0x{{.+}} class d_mat definition
// AST-NEXT: TemplateArgument type 'float'
// AST-NEXT: TemplateArgument integral 3
// AST-NEXT: TemplateArgument integral 2
// AST-NEXT: CXXRecordDecl 0x{{.+}} implicit class d_mat
// AST-NEXT: FieldDecl 0x[[M_FIELD:[0-9a-f]+]] {{.+}} referenced m 'matrix<float, 3, 2>'

// AST: ClassTemplateDecl 0x{{.+}} d_mat2
// AST-NEXT: TemplateTypeParmDecl 0x{{.+}} referenced typename T
// AST-NEXT: NonTypeTemplateParmDecl 0x{{.+}} referenced 'int' r
// AST-NEXT: CXXRecordDecl 0x{{.+}} class d_mat2 definition
// AST-NEXT: CXXRecordDecl 0x{{.+}} implicit class d_mat2
// AST-NEXT: FieldDecl 0x[[D2_FIELD0:[0-9a-f]+]] {{.+}} referenced d2 'd_mat<T, r, 2>'
// AST-NEXT: CXXMethodDecl 0x{{.+}} add 'matrix<T, r, 2> (d_mat<T, r, 2>)'
// AST-NEXT: ParmVarDecl 0x[[D_VAR_DECL0:[0-9a-f]+]] {{.+}} referenced d 'd_mat<T, r, 2>'
// AST-NEXT: CompoundStmt
// AST-NEXT: ReturnStmt
// AST-NEXT: BinaryOperator 0x{{.+}} '<dependent type>' '+'
// AST-NEXT: CXXDependentScopeMemberExpr 0x{{.+}} '<dependent type>' lvalue
// AST-NEXT: MemberExpr 0x{{.+}} lvalue .d2 0x[[D2_FIELD0]]
// AST-NEXT: CXXThisExpr 0x{{.+}} 'd_mat2<T, r>' lvalue this
// AST-NEXT: CXXDependentScopeMemberExpr 0x{{.+}} '<dependent type>' lvalue
// AST-NEXT: DeclRefExpr 0x{{.+}} 'd_mat<T, r, 2>' lvalue ParmVar 0x[[D_VAR_DECL0]] 'd' 'd_mat<T, r, 2>'
// AST-NEXT: ClassTemplateSpecializationDecl 0x{{.+}} class d_mat2 definition
// AST-NEXT: TemplateArgument type 'float'
// AST-NEXT: TemplateArgument integral 3
// AST-NEXT: CXXRecordDecl 0x{{.+}} implicit class d_mat2
// AST-NEXT: FieldDecl 0x[[D2_FIELD:[0-9a-f]+]] {{.+}} referenced d2 'd_mat<float, 3, 2>':'d_mat<float, 3, 2>'
// AST-NEXT: CXXMethodDecl 0x{{.+}} used add 'matrix<float, 3, 2> (d_mat<float, 3, 2>)'
// AST-NEXT: ParmVarDecl 0x[[D_VAR_DECL:[0-9a-f]+]] {{.+}} used d 'd_mat<float, 3, 2>':'d_mat<float, 3, 2>'
// AST-NEXT: CompoundStmt
// AST-NEXT: ReturnStmt
// AST-NEXT: BinaryOperator 0x{{.+}} 'matrix<float, 3, 2>' '+'
// AST-NEXT: ImplicitCastExpr 0x{{.+}} 'matrix<float, 3, 2>' <LValueToRValue>
// AST-NEXT: MemberExpr 0x{{.+}} 'matrix<float, 3, 2>' lvalue .m 0x[[M_FIELD]]
// AST-NEXT: MemberExpr 0x{{.+}} 'd_mat<float, 3, 2>':'d_mat<float, 3, 2>' lvalue .d2 0x[[D2_FIELD]]
// AST-NEXT: CXXThisExpr 0x{{.+}} 'd_mat2<float, 3>' lvalue this
// AST-NEXT: ImplicitCastExpr 0x{{.+}} 'matrix<float, 3, 2>' <LValueToRValue>
// AST-NEXT: MemberExpr 0x{{.+}} 'matrix<float, 3, 2>' lvalue .m 0x[[M_FIELD]]
// AST-NEXT: DeclRefExpr 0x{{.+}} 'd_mat<float, 3, 2>':'d_mat<float, 3, 2>' lvalue ParmVar 0x[[D_VAR_DECL]] 'd' 'd_mat<float, 3, 2>':'d_mat<float, 3, 2>'

template<typename T, int r, int c>
class d_mat {
  matrix<T, r, c> m;
};

template<typename T, int r>
class d_mat2 {
  d_mat<T, r, 2> d2;

  matrix<T, r, 2> add(d_mat<T, r, 2> d) {
    return d2.m + d.m;
  }

};

template<typename T, int r>
vector<T, r> tmul(d_mat<T, r, 2> d, vector<T, 2> v) {
  return mul(v, d.m);
}

template<typename T, int r, int c>
vector<T, c> test(matrix<T, r, c> a) {
  return a[2];
}

// CHECK: define float @"\01?bar@@YAMV?$d_mat@M$01$01@@V?$vector@M$01@@@Z"(%"class.d_mat<float, 2, 2>"* %d, <2 x float> %v)
// CHECK: call <2 x float> @"\01??$tmul@M$01@@YA?AV?$vector@M$01@@V?$d_mat@M$01$01@@V0@@Z"(

// CHECK: define linkonce_odr <2 x float> @"\01??$tmul@M$01@@YA?AV?$vector@M$01@@V?$d_mat@M$01$01@@V0@@Z"(
// CHECK:call <2 x float> @"dx.hl.op..<2 x float> (i32, <2 x float>, %class.matrix.float.2.2)"(i32 165,

export float bar(d_mat<float, 2, 2> d, float2 v) {
  
  return tmul<float, 2>(d, v).y;
}

// CHECK: define float @"\01?bar@@YAMV?$d_mat2@M$02@@0@Z"(%"class.d_mat2<float, 3>"* %d, %"class.d_mat2<float, 3>"* %d1)
// CHECK: call %class.matrix.float.3.2 @"\01?add@?$d_mat2@M$02@@QAAV?$matrix@M$02$01@@V?$d_mat@M$02$01@@@Z"(
// CHECK: call <2 x float> @"\01??$test@M$02$01@@YA?AV?$vector@M$01@@V?$matrix@M$02$01@@@Z"(
// CHECK: define linkonce_odr <2 x float> @"\01??$test@M$02$01@@YA?AV?$vector@M$01@@V?$matrix@M$02$01@@@Z"(%class.matrix.float.3.2 %a)
// CHECK:  call <2 x float>* @"dx.hl.subscript.colMajor[].<2 x float>* (i32, %class.matrix.float.3.2*, i32, i32)"(i32 1,
// CHECK: define linkonce_odr %class.matrix.float.3.2 @"\01?add@?$d_mat2@M$02@@QAAV?$matrix@M$02$01@@V?$d_mat@M$02$01@@@Z"(%"class.d_mat2<float, 3>"* %this, %"class.d_mat<float, 3, 2>"* %d)
// CHECK: call %class.matrix.float.3.2 @"dx.hl.binop.+.%class.matrix.float.3.2 (i32, %class.matrix.float.3.2, %class.matrix.float.3.2)"(i32 4,
export float bar(d_mat2<float, 3> d, d_mat2<float, 3> d1) {
  return test(d.add(d1.d2)).y;
}

// CHECK:define float @"\01?foo@@YAMXZ"()
// CHECK:%[[GCB_HANDLE:.+]] = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22$Globals\22*, i32)"(i32 0, %"$Globals"* @"$Globals", i32 0)
// CHECK:%[[GCB_ANNOT_HANDLE:.+]] = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22$Globals\22)"(i32 11, %dx.types.Handle %[[GCB_HANDLE]], %dx.types.ResourceProperties { i32 13, i32 28 }, %"$Globals" undef)
// CHECK:%[[GCB_SUBSCRIPT:.+]] = call %"$Globals"* @"dx.hl.subscript.cb.%\22$Globals\22* (i32, %dx.types.Handle, i32)"(i32 6, %dx.types.Handle %[[GCB_ANNOT_HANDLE]], i32 0)
// CHECK:%[[GCB_SUB_PTR:.+]] = getelementptr inbounds %"$Globals", %"$Globals"* %[[GCB_SUBSCRIPT]], i32 0, i32 0, i32 0, i32 0
// CHECK:%[[GCB_MAT_SUB:.+]] = call <2 x float>* @"dx.hl.subscript.colMajor[].<2 x float>* (i32, %class.matrix.float.3.2*, i32, i32)"(i32 1, %class.matrix.float.3.2* %[[GCB_SUB_PTR]], i32 0, i32 3)
// CHECK:%[[GCB_MAT_SUB_V:.+]] = load <2 x float>, <2 x float>* %[[GCB_MAT_SUB]], align 4
// CHECK:%[[RET_V:.+]] = extractelement <2 x float> %[[GCB_MAT_SUB_V]], i32 1
// CHECK:ret float %[[RET_V]]
d_mat2<float, 3> m3;

export float foo() {
  return m3.d2.m[0].g;
}
