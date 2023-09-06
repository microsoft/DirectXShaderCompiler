// RUN: %dxc -T cs_6_6 -E mymain -ast-dump %s | FileCheck %s

// the purpose of this test is to verify that the implicit 
// compute hlsl shader attribute doesn't get incorrectly 
// added to declarations that are not the global-level
// entry point declaration, and the attribute should
// really only be added to exactly one decl.

// this parses through the class definition
// CHECK: -CXXRecordDecl
// CHECK: -HLSLNumThreadsAttr

// after the class, there should be no
// shader attribute, so we go to the next decl,
// the namespace decl:
// CHECK-NEXT: -NamespaceDecl
// CHECK: -ReturnStmt

// Now, we should check that the attribute got added correctly to the 
// entry point decl at the global level:

// CHECK: mymain 'void ()'
// CHECK: -HLSLShaderAttr
// CHECK-SAME: "compute"

class foo {
	[numthreads(1,1,1)]
	void mymain(){
		return;
	}
};

namespace bar {
	void mymain(){
		return;
	}
};

[numthreads(1,1,1)]
void mymain(){
	return;
}
