// RUN: %dxc -T lib_6_8 -ast-dump-implicit %s | FileCheck %s
// This test verifies the AST of the EmptyNodeOutputArray type. The
// source doesn't matter except that it forces a use to ensure the AST is fully
// loaded by the external sema source.

struct RECORD1
{
  uint value;
  uint value2;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
void node_2_0(
    [AllowSparseNodes] [NodeArraySize(131)] [MaxRecords(41)]
    EmptyNodeOutputArray OutputArray_2_0) {
  OutputArray_2_0[1].GroupIncrementOutputCount(10);
}

// CHECK:|-CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit struct EmptyNodeOutput definition
// CHECK-NEXT:| |-FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// CHECK-NEXT:| |-HLSLNodeObjectAttr 0x{{.+}} <<invalid sloc>> Implicit EmptyNodeOutput
// CHECK-NEXT:| |-FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit h 'int'
// CHECK-NEXT:| |-FunctionTemplateDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> GroupIncrementOutputCount
// CHECK-NEXT:| | |-TemplateTypeParmDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> class TResult
// CHECK-NEXT:| | |-TemplateTypeParmDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> class Tcount
// CHECK-NEXT:| | |-CXXMethodDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit GroupIncrementOutputCount 'TResult (Tcount) const'
// CHECK-NEXT:| | | `-ParmVarDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> count 'Tcount'
// CHECK-NEXT:| | `-CXXMethodDecl 0x[[GroupIncrementOutputCount:[0-9a-f]+]] <<invalid sloc>> <invalid sloc> used GroupIncrementOutputCount 'void (unsigned int)' extern
// CHECK-NEXT:| |   |-TemplateArgument type 'void'
// CHECK-NEXT:| |   |-TemplateArgument type 'unsigned int'
// CHECK-NEXT:| |   |-ParmVarDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> GroupIncrementOutputCount 'unsigned int'
// CHECK-NEXT:| |   `-HLSLIntrinsicAttr 0x{{.+}} <<invalid sloc>> Implicit "op" "" 338
// CHECK-NEXT:| |-FunctionTemplateDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> IsValid
// CHECK-NEXT:| | |-TemplateTypeParmDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> class TResult
// CHECK-NEXT:| | `-CXXMethodDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit IsValid 'TResult () const'
// CHECK-NEXT:| |-FunctionTemplateDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> ThreadIncrementOutputCount
// CHECK-NEXT:| | |-TemplateTypeParmDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> class TResult
// CHECK-NEXT:| | |-TemplateTypeParmDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> class Tcount
// CHECK-NEXT:| | `-CXXMethodDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit ThreadIncrementOutputCount 'TResult (Tcount) const'
// CHECK-NEXT:| |   `-ParmVarDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> count 'Tcount'
// CHECK-NEXT:| `-CXXDestructorDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced ~EmptyNodeOutput 'void () noexcept' inline

// CHECK:|-CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced struct EmptyNodeOutputArray definition
// CHECK-NEXT:| |-FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// CHECK-NEXT:| |-HLSLNodeObjectAttr 0x{{.+}} <<invalid sloc>> Implicit EmptyNodeOutputArray
// CHECK-NEXT:| |-FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit h 'int'
// CHECK-NEXT:| `-CXXMethodDecl 0x[[SUB:[0-9a-f]+]] <<invalid sloc>> <invalid sloc> used operator[] 'EmptyNodeOutput (unsigned int)'
// CHECK-NEXT:|   |-ParmVarDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> index 'unsigned int'
// CHECK-NEXT:|   |-HLSLIntrinsicAttr 0x{{.+}} <<invalid sloc>> Implicit "indexnodehandle" "" 12
// CHECK-NEXT:|   `-HLSLCXXOverloadAttr 0x{{.+}} <<invalid sloc>> Implicit

// CHECK: `-FunctionDecl 0x{{.+}} node_2_0 'void (EmptyNodeOutputArray)'
// CHECK:   |-ParmVarDecl 0x[[Param:[0-9a-f]+]] {{.+}} used OutputArray_2_0 'EmptyNodeOutputArray'
// CHECK:   | |-HLSLMaxRecordsAttr 0x{{.+}} 41
// CHECK:   | |-HLSLNodeArraySizeAttr 0x{{.+}} <col:25, col:42> 131
// CHECK:   | `-HLSLAllowSparseNodesAttr 0x{{.+}} <col:6>
// CHECK:   |-CompoundStmt 0x{{.+}}
// CHECK:   | `-CXXMemberCallExpr 0x{{.+}} 'void'
// CHECK:   |   |-MemberExpr 0x{{.+}} <col:3, col:22> '<bound member function type>' .GroupIncrementOutputCount 0x[[GroupIncrementOutputCount]]
// CHECK:   |   | `-CXXOperatorCallExpr 0x{{.+}} <col:3, col:20> 'EmptyNodeOutput'
// CHECK:   |   |   |-ImplicitCastExpr 0x{{.+}} <col:18, col:20> 'EmptyNodeOutput (*)(unsigned int)' <FunctionToPointerDecay>
// CHECK:   |   |   | `-DeclRefExpr 0x{{.+}} <col:18, col:20> 'EmptyNodeOutput (unsigned int)' lvalue CXXMethod 0x[[SUB]] 'operator[]' 'EmptyNodeOutput (unsigned int)'
// CHECK:   |   |   |-DeclRefExpr 0x{{.+}} <col:3> 'EmptyNodeOutputArray' lvalue ParmVar 0x[[Param]] 'OutputArray_2_0' 'EmptyNodeOutputArray'
// CHECK:   |   |   `-ImplicitCastExpr 0x{{.+}} <col:19> 'unsigned int' <IntegralCast>
// CHECK:   |   |     `-IntegerLiteral 0x{{.+}}{{.+}} <col:19> 'literal int' 1
// CHECK:   |   `-ImplicitCastExpr 0x{{.+}} <col:48> 'unsigned int' <IntegralCast>
// CHECK:   |     `-IntegerLiteral 0x{{.+}} <col:48> 'literal int' 10
// CHECK:   |-HLSLNumThreadsAttr 0x{{.+}} 1 1 1
// CHECK:   |-HLSLNodeDispatchGridAttr 0x{{.+}} 1 1 1
// CHECK:   |-HLSLNodeLaunchAttr 0x{{.+}} "broadcasting"
// CHECK:   `-HLSLShaderAttr 0x{{.+}} "node"
