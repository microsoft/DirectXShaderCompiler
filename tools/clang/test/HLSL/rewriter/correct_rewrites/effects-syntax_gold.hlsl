// Rewrite unchanged result:



Texture2D tex : register(t1), tex2 : register(t2)







< int foo=1; >
{
    Texture = tex;
    Filter = MIN_MAG_MIP_LINEAR;
    MaxAnisotropy = 0;
    AddressU = Wrap;
    AddressV = Wrap;
}, texa[3]
<
  string Name = "texa";
  int ArraySize = 3;
>;

Texture texCaps;

SamplerState samLinear : register(s7)




< bool foo=1 > 2; >
{
    Texture = tex;
    Filter = MIN_MAG_MIP_LINEAR;
    MaxAnisotropy = 0;
    AddressU = Wrap;
    AddressV = Wrap;
};


sampler S : register(s1) = sampler_state {texture=tex;};




SamplerComparisonState SC : register(s3) = sampler_state {texture=tex;};

float4 main() : SV_Target
{


  {
    int StateBlock = 1;
  }


  int PixelShadeR = 1;
  RenderTargetView rtv { state=foo; };




  Texture2D l_tex { state=foo; };






  int foobar {blah=foo;};




  int RenderTargetView = 1;
  int PixelShader = 1;
  int pixelshader = 1;



  int TechNiQue = 1;

  return tex.Sample(samLinear, float2(0.1,0.2));
}




texture tex1 < int foo=1; > { state=foo; };
static const PixelShader ps1 { state=foo; };
pixelfragment pfrag;
vertexfragment vfrag;
ComputeShader cs;
DomainShader ds;
GeometryShader gs;
HullShader hs;
BlendState BS { state=foo; };
DepthStencilState DSS { state=foo; };
DepthStencilView SDV;
RasterizerState RS { state=foo; };
RenderTargetView RTV < int foo=1;> ;


technique T0



{
  pass {}
}
Technique
{
  pass {}
}


technique10 T10



{
  pass {}
}

technique10
{
  pass {}
}
int foobar3;
int foobar4;
