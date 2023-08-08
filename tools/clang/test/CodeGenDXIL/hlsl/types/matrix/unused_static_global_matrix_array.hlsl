// RUN: %dxc -E main -T vs_6_0 %s -fcgl | FileCheck %s

// Regression test for github issue #3579

// %C = type { [1 x %class.matrix.float.1.1] }
// @bar = internal global [1 x %class.matrix.float.1.1] undef

// CHECK: %[[CB:.+]] = call %C* @"dx.hl.subscript.cb.%C* (i32, %dx.types.Handle, i32)"(i32 6, %dx.types.Handle %{{.+}}, i32 0)
// CHECK: %[[CB_PTR:.+]] = getelementptr inbounds %C, %C* %[[CB]], i32 0, i32 0
// CHECK: %[[BAR_ADDR:.+]] = bitcast [1 x %class.matrix.float.1.1]* @bar to i8*
// CHECK: %[[CB_ADDR:.+]] = bitcast [1 x %class.matrix.float.1.1]* %[[CB_PTR]] to i8*
// CHECK:  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %[[BAR_ADDR]], i8* %[[CB_ADDR]], i64 4, i32 1, i1 false)


cbuffer C
{
 float1x1 foo[1];
}

static const float1x1 bar[1] = foo;

// NOTE: Commented out root signature to work around https://github.com/microsoft/DirectXShaderCompiler/issues/5476
// [RootSignature("DescriptorTable(SRV(t0), UAV(u0))")]
void main() {}

