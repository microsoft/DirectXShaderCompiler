// RUN: %dxc -T lib_6_6 -DDEF -Fo static_def %s | FileCheck %s -check-prefix=DEF
// RUN: %dxc -T lib_6_6 -Fo static_use %s | FileCheck %s -check-prefix=USE

// RUN: %dxl -T lib_6_6 static_def;static_use %s  | FileCheck %s -check-prefix=LINK_LIB
// RUN: %dxl -T ps_6_6 static_def;static_use -Etest %s  | FileCheck %s -check-prefix=LINK_PS

// This test is to show global ctor required for exported function.

// Make sure global ctors exist in DEF.
// DEF:@llvm.global_ctors = appending global [1 x { i32, void ()*, i8* }] [{ i32, void ()*, i8* } { i32 65535, void ()* @_GLOBAL__sub_I_link_static_global_init.hlsl, i8* null }]

// Make sure no cbuf in USE.
// USE:target triple = "dxil-ms-dx"
// USE-NOT:call %dx.types.CBufRet.f32

// Make sure global ctors exist when link to lib.
// LINK_LIB:@llvm.global_ctors = appending global [1 x { i32, void ()*, i8* }] [{ i32, void ()*, i8* } { i32 65535, void ()* @static_def._GLOBAL__sub_I_link_static_global_init.hlsl, i8* null }]

// Make sure global ctors removed when link to ps.
// LINK_PS-NOT:@llvm.global_ctors
// Make sure cbuffer load is generated for the init of g[1].
// LINK_PS:call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32



#ifdef DEF

cbuffer X {
  float f;
}

static float g[2] = { 1, f };

export
float update() {
  return g[1]++;
}

#else

float update();

[shader("pixel")]
float test() : SV_Target {
  return update();
}

#endif