// RUN: %dxc -Wno-effects-syntax -E MainPs -T ps_6_0 
string g_renderState_FillMode < string arg1 = "WIREFRAME" ; > = "" ;
struct PS_OUTPUT
{
    float4 vColor0 : SV_Target0 ;
} ;

// CHECK: {{%\d+}} = OpString ""

PS_OUTPUT MainPs (  )
{
    PS_OUTPUT o ;
    o . vColor0 = float4( 0, 0, 0, 1 );
    return o ;
}

