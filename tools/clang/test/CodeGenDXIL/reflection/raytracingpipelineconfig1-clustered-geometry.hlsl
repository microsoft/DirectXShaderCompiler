// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 %s -Fo %t -Fc %t.ll && FileCheck %s --input-file=%t.ll
// RUN: %dxc -T lib_6_10 -fcgl %s | FileCheck -check-prefix=FCGL %s
// RUN: not %dxc -T lib_6_9 -WX %s 2>&1 | FileCheck -check-prefix=SM69 %s
// RUN: %dxa %t -dumprdat | FileCheck -check-prefix=RDAT %s

// CHECK: ; RaytracingPipelineConfig1 clustered = { MaxTraceRecursionDepth = 1, Flags = RAYTRACING_PIPELINE_FLAG_ALLOW_CLUSTERED_GEOMETRY };
// CHECK: ; RaytracingPipelineConfig1 combined = { MaxTraceRecursionDepth = 1, Flags = RAYTRACING_PIPELINE_FLAG_SKIP_TRIANGLES | RAYTRACING_PIPELINE_FLAG_ALLOW_OPACITY_MICROMAPS | RAYTRACING_PIPELINE_FLAG_ALLOW_CLUSTERED_GEOMETRY };

// FCGL: !{!"clustered", i32 12, i32 1, i32 2048}
// FCGL: !{!"combined", i32 12, i32 1, i32 3328}

// SM69: error: potential misuse of built-in constant 'RAYTRACING_PIPELINE_FLAG_ALLOW_CLUSTERED_GEOMETRY' in shader model lib_6_9; introduced in shader model 6.10

// RDAT: SubobjectTable[2]
// RDAT: Name: "clustered"
// RDAT: Flags: (AllowClusteredGeometry)
// RDAT: Name: "combined"
// RDAT: Flags: (SkipTriangles | AllowOpacityMicromaps | AllowClusteredGeometry)

RaytracingPipelineConfig1 clustered = {
    1, RAYTRACING_PIPELINE_FLAG_ALLOW_CLUSTERED_GEOMETRY};

static const uint CombinedFlags =
    RAYTRACING_PIPELINE_FLAG_SKIP_TRIANGLES |
    RAYTRACING_PIPELINE_FLAG_ALLOW_OPACITY_MICROMAPS |
    RAYTRACING_PIPELINE_FLAG_ALLOW_CLUSTERED_GEOMETRY;

RaytracingPipelineConfig1 combined = {1, CombinedFlags};

[shader("raygeneration")]
void main() {}
