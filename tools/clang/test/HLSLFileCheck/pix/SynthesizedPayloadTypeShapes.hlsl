// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=16,expanded-payload-offset=4 | %FileCheck %s -check-prefixes=EXACTFIT
// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=32,expanded-payload-offset=4 | %FileCheck %s -check-prefixes=TAILPADDING
// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=12,expanded-payload-offset=0 | %FileCheck %s -check-prefixes=EMPTYPAYLOAD
// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=28,expanded-payload-offset=16 | %FileCheck %s -check-prefixes=MISMATCHEDLAYOUT
// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=16384,expanded-payload-offset=16368 | %FileCheck %s -check-prefixes=MAXPAYLOAD

// EXACTFIT: %PIX_AS2MS_Expanded_Type = type { [1 x i32], i32, i32, i32 }
// EXACTFIT: [[PAYLOAD:%[0-9]+]] = call %PIX_AS2MS_Expanded_Type* @dx.op.getMeshPayload.PIX_AS2MS_Expanded_Type(i32 170)
// EXACTFIT: getelementptr %PIX_AS2MS_Expanded_Type, %PIX_AS2MS_Expanded_Type* [[PAYLOAD]], i32 0, i32 1
// EXACTFIT: getelementptr %PIX_AS2MS_Expanded_Type, %PIX_AS2MS_Expanded_Type* [[PAYLOAD]], i32 0, i32 2
// EXACTFIT: getelementptr %PIX_AS2MS_Expanded_Type, %PIX_AS2MS_Expanded_Type* [[PAYLOAD]], i32 0, i32 3
// EXACTFIT: = !{{{![0-9]+}}, i32 4, i32 2, i32 2, i32 16}
// TAILPADDING: %PIX_AS2MS_Expanded_Type = type { [1 x i32], i32, i32, i32, [4 x i32] }
// TAILPADDING: = !{{{![0-9]+}}, i32 4, i32 2, i32 2, i32 32}
// EMPTYPAYLOAD: %PIX_AS2MS_Expanded_Type = type { [0 x i32], i32, i32, i32 }
// EMPTYPAYLOAD: [[EMPTY:%[0-9]+]] = call %PIX_AS2MS_Expanded_Type* @dx.op.getMeshPayload.PIX_AS2MS_Expanded_Type(i32 170)
// EMPTYPAYLOAD: getelementptr %PIX_AS2MS_Expanded_Type, %PIX_AS2MS_Expanded_Type* [[EMPTY]], i32 0, i32 1
// EMPTYPAYLOAD: = !{{{![0-9]+}}, i32 4, i32 2, i32 2, i32 12}
// MISMATCHEDLAYOUT: %PIX_AS2MS_Expanded_Type = type { [4 x i32], i32, i32, i32 }
// MISMATCHEDLAYOUT: = !{{{![0-9]+}}, i32 4, i32 2, i32 2, i32 28}
// MAXPAYLOAD: %PIX_AS2MS_Expanded_Type = type { [4092 x i32], i32, i32, i32, [1 x i32] }
// MAXPAYLOAD: = !{{{![0-9]+}}, i32 4, i32 2, i32 2, i32 16384}

struct PSInput
{
    float4 position : SV_POSITION;
};

struct Payload
{
    uint value;
};

[outputtopology("triangle")]
[numthreads(4, 1, 1)]
void MSMain(
    in payload Payload payload,
    in uint tid : SV_GroupThreadID,
    out vertices PSInput verts[4],
    out indices uint3 triangles[2])
{
    SetMeshOutputCounts(4, 2);
    verts[tid].position = float4(0, 0, 0, 0);
    triangles[tid % 2] = uint3(0, tid + 1, tid + 2);
}
