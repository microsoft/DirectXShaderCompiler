// RUN: %dxc -T ps_6_0 -E main

// CHECK: OpCapability StencilExportEXT

// CHECK: OpExtension "SPV_EXT_shader_stencil_export"

// TODO: we may need to check the StencilRefReplacingEXT execution mode here.

// CHECK: OpEntryPoint Fragment %main "main" [[StencilRef:%\d+]]

// CEHCK: OpDecorate [[StencilRef]] BuiltIn FragStencilRefEXT

// CHECK: [[StencilRef]] = OpVariable %_ptr_Output_uint Output

uint main() : SV_StencilRef {
    return 3;
}
