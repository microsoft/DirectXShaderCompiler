// Rewrite unchanged result:


float4 FetchFromIndexMap( uniform Texture2D Tex, uniform SamplerState SS, const float2 RoundedUV, const float LOD )
{
    float4 Sample = Tex.SampleLevel( SS, RoundedUV, LOD );
    return Sample * 255.0f;
}
struct VS_IN
{
   float2 Pos : POSITION;
   uint2 TexAndChannelSelector : TEXCOORD0;
};

struct VS_OUT
{
   float4 Position : POSITION;
   float4 Diffuse : COLOR0_center;
   float2 TexCoord0 : TEXCOORD0;
   float4 ChannelSelector : TEXCOORD1;
};


cbuffer OncePerDrawText : register( b0 )
{
    float2 TexScale : register( c0 );
    float4 Color : register( c1 );
};





sampler FontTexture;

VS_OUT FontVertexShader( VS_IN In )
{
    VS_OUT Out;

    Out.Position.x = In.Pos.x;
    Out.Position.y = In.Pos.y;


    Out.Diffuse = Color;

    uint texX = (In.TexAndChannelSelector.x >> 0 ) & 0xffff;
    uint texY = (In.TexAndChannelSelector.x >> 16) & 0xffff;
    Out.TexCoord0 = float2( texX, texY ) * TexScale;

    Out.ChannelSelector.w = (0 != (In.TexAndChannelSelector.y & 0xf000)) ? 1 : 0;
    Out.ChannelSelector.x = (0 != (In.TexAndChannelSelector.y & 0x0f00)) ? 1 : 0;
    Out.ChannelSelector.y = (0 != (In.TexAndChannelSelector.y & 0x00f0)) ? 1 : 0;
    Out.ChannelSelector.z = (0 != (In.TexAndChannelSelector.y & 0x000f)) ? 1 : 0;

    return Out;
}

float4 FontPixelShader( VS_OUT In ) : COLOR0
{

    float4 FontTexel = tex2D( FontTexture, In.TexCoord0 ).zyxw;

    float4 Color = FontTexel * In.Diffuse;

    if( dot( In.ChannelSelector, 1 ) )
    {

        float value = dot( FontTexel, In.ChannelSelector );
        Color.rgb = ( value > 0.5f ? 2*value-1 : 0.0f );
        Color.a = 2 * ( value > 0.5f ? 1.0f : value );



        Color *= In.Diffuse;
    }

    clip( Color.a - (8.f / 255.f) );

    return Color;
};
