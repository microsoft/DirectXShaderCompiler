// RUN: %dxilver 1.9 | %dxc -T lib_6_9  -ast-dump-implicit %s | FileCheck -check-prefix=ASTIMPL %s
// The HLSL source is just a copy of 
// tools\clang\test\HLSLFileCheck\shader_targets\raytracing\subobjects_raytracingPipelineConfig1.hlsl

// This test tests that the HLSLSubObjectAttr attribute is present on all
// HLSL subobjects

// ASTIMPL: CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced struct StateObjectConfig definition
// ASTIMPL-NEXT: HLSLSubObjectAttr 0x{{.+}} <<invalid sloc>> Implicit 0 -1
// ASTIMPL-NEXT: FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit Flags 'unsigned int'
// ASTIMPL-NEXT: CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced struct GlobalRootSignature definition
// ASTIMPL-NEXT: HLSLSubObjectAttr 0x{{.+}} <<invalid sloc>> Implicit 1 -1
// ASTIMPL-NEXT: FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit Data 'string'
// ASTIMPL-NEXT: CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced struct LocalRootSignature definition
// ASTIMPL-NEXT: HLSLSubObjectAttr 0x{{.+}} <<invalid sloc>> Implicit 2 -1
// ASTIMPL-NEXT: FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit Data 'string'
// ASTIMPL-NEXT: CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced struct SubobjectToExportsAssociation definition
// ASTIMPL-NEXT: HLSLSubObjectAttr 0x{{.+}} <<invalid sloc>> Implicit 8 -1
// ASTIMPL-NEXT: FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit Subobject 'string'
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit Exports 'string'
// ASTIMPL-NEXT: CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced struct RaytracingShaderConfig definition
// ASTIMPL-NEXT: HLSLSubObjectAttr 0x{{.+}} <<invalid sloc>> Implicit 9 -1
// ASTIMPL-NEXT: FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit MaxPayloadSizeInBytes 'unsigned int'
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit MaxAttributeSizeInBytes 'unsigned int'
// ASTIMPL-NEXT: CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit struct RaytracingPipelineConfig definition
// ASTIMPL-NEXT: HLSLSubObjectAttr 0x{{.+}} <<invalid sloc>> Implicit 10 -1
// ASTIMPL-NEXT: FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit MaxTraceRecursionDepth 'unsigned int'
// ASTIMPL-NEXT: CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced struct TriangleHitGroup definition
// ASTIMPL-NEXT: HLSLSubObjectAttr 0x{{.+}} <<invalid sloc>> Implicit 11 0
// ASTIMPL-NEXT: FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit AnyHit 'string'
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit ClosestHit 'string'
// ASTIMPL-NEXT: CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced struct ProceduralPrimitiveHitGroup definition
// ASTIMPL-NEXT: HLSLSubObjectAttr 0x{{.+}} <<invalid sloc>> Implicit 11 1
// ASTIMPL-NEXT: FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit AnyHit 'string'
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit ClosestHit 'string'
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit Intersection 'string'
// ASTIMPL-NEXT: CXXRecordDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit referenced struct RaytracingPipelineConfig1 definition
// ASTIMPL-NEXT: HLSLSubObjectAttr 0x{{.+}} <<invalid sloc>> Implicit 12 -1
// ASTIMPL-NEXT: FinalAttr 0x{{.+}} <<invalid sloc>> Implicit final
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit MaxTraceRecursionDepth 'unsigned int'
// ASTIMPL-NEXT: FieldDecl 0x{{.+}} <<invalid sloc>> <invalid sloc> implicit Flags 'unsigned int'


GlobalRootSignature grs = {"CBV(b0)"};	
StateObjectConfig soc = { STATE_OBJECT_FLAGS_ALLOW_LOCAL_DEPENDENCIES_ON_EXTERNAL_DEFINITONS | STATE_OBJECT_FLAG_ALLOW_STATE_OBJECT_ADDITIONS };
LocalRootSignature lrs = {"UAV(u0, visibility = SHADER_VISIBILITY_GEOMETRY), RootFlags(LOCAL_ROOT_SIGNATURE)"};
SubobjectToExportsAssociation sea = { "grs", "a;b;foo;c" };
// Empty association is well-defined: it creates a default association
SubobjectToExportsAssociation sea2 = { "grs", ";" };
SubobjectToExportsAssociation sea3 = { "grs", "" };
RaytracingShaderConfig rsc = { 128, 64 };
RaytracingPipelineConfig1 rpc = { 32, RAYTRACING_PIPELINE_FLAG_SKIP_TRIANGLES };
SubobjectToExportsAssociation sea4 = {"rpc", ";"};
RaytracingPipelineConfig1 rpc2 = {32, RAYTRACING_PIPELINE_FLAG_NONE };
TriangleHitGroup trHitGt = {"a", "b"};
ProceduralPrimitiveHitGroup ppHitGt = { "a", "b", "c"}; //

// DXR entry point to ensure RDAT flags match during validation.
[shader("raygeneration")]
void main(void) {
}
