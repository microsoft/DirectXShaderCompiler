// Run: %dxc -T ps_6_0 -E main -fspv-reflect

// CHECK: OpExtension "SPV_GOOGLE_hlsl_functionality1"
// CHECK: OpExtension "SPV_GOOGLE_user_type"

// CHECK: OpDecorateString %a UserTypeGOOGLE "structuredbuffer"
StructuredBuffer<float> a;
// CHECK: OpDecorateString %b UserTypeGOOGLE "rwstructuredbuffer"
RWStructuredBuffer<float> b;
// CHECK: OpDecorateString %c UserTypeGOOGLE "appendstructuredbuffer"
AppendStructuredBuffer<float> c;
// CHECK: OpDecorateString %d UserTypeGOOGLE "consumestructuredbuffer"
ConsumeStructuredBuffer<float> d;
// CHECK: OpDecorateString %e UserTypeGOOGLE "texture1d"
Texture1D<float> e;
// CHECK: OpDecorateString %f UserTypeGOOGLE "texture2d"
Texture2D<float> f;
// CHECK: OpDecorateString %g UserTypeGOOGLE "texture3d"
Texture3D<float> g;
// CHECK: OpDecorateString %h UserTypeGOOGLE "texturecube"
TextureCube<float> h;
// CHECK: OpDecorateString %i UserTypeGOOGLE "texture1darray"
Texture1DArray<float> i;
// CHECK: OpDecorateString %j UserTypeGOOGLE "texture2darray"
Texture2DArray<float> j;
// CHECK: OpDecorateString %k UserTypeGOOGLE "texture2dms"
Texture2DMS<float> k;
// CHECK: OpDecorateString %l UserTypeGOOGLE "texture2dmsarray"
Texture2DMSArray<float> l;
// CHECK: OpDecorateString %m UserTypeGOOGLE "texturecubearray"
TextureCubeArray<float> m;
// CHECK: OpDecorateString %n UserTypeGOOGLE "rwtexture1d"
RWTexture1D<float> n;
// CHECK: OpDecorateString %o UserTypeGOOGLE "rwtexture2d"
RWTexture2D<float> o;
// CHECK: OpDecorateString %p UserTypeGOOGLE "rwtexture3d"
RWTexture3D<float> p;
// CHECK: OpDecorateString %q UserTypeGOOGLE "rwtexture1darray"
RWTexture1DArray<float> q;
// CHECK: OpDecorateString %r UserTypeGOOGLE "rwtexture2darray"
RWTexture2DArray<float> r;
// CHECK: OpDecorateString %s UserTypeGOOGLE "buffer"
Buffer<float> s;
// CHECK: OpDecorateString %t UserTypeGOOGLE "rwbuffer"
RWBuffer<float> t;

// CHECK: OpDecorateString %eArr UserTypeGOOGLE "texture1d"
Texture1D<float> eArr[5];
// CHECK: OpDecorateString %fArr UserTypeGOOGLE "texture2d"
Texture2D<float> fArr[5];
// CHECK: OpDecorateString %gArr UserTypeGOOGLE "texture3d"
Texture3D<float> gArr[5];
// CHECK: OpDecorateString %hArr UserTypeGOOGLE "texturecube"
TextureCube<float> hArr[5];
// CHECK: OpDecorateString %iArr UserTypeGOOGLE "texture1darray"
Texture1DArray<float> iArr[5];
// CHECK: OpDecorateString %jArr UserTypeGOOGLE "texture2darray"
Texture2DArray<float> jArr[5];
// CHECK: OpDecorateString %kArr UserTypeGOOGLE "texture2dms"
Texture2DMS<float> kArr[5];
// CHECK: OpDecorateString %lArr UserTypeGOOGLE "texture2dmsarray"
Texture2DMSArray<float> lArr[5];
// CHECK: OpDecorateString %mArr UserTypeGOOGLE "texturecubearray"
TextureCubeArray<float> mArr[5];
// CHECK: OpDecorateString %nArr UserTypeGOOGLE "rwtexture1d"
RWTexture1D<float> nArr[5];
// CHECK: OpDecorateString %oArr UserTypeGOOGLE "rwtexture2d"
RWTexture2D<float> oArr[5];
// CHECK: OpDecorateString %pArr UserTypeGOOGLE "rwtexture3d"
RWTexture3D<float> pArr[5];
// CHECK: OpDecorateString %qArr UserTypeGOOGLE "rwtexture1darray"
RWTexture1DArray<float> qArr[5];
// CHECK: OpDecorateString %rArr UserTypeGOOGLE "rwtexture2darray"
RWTexture2DArray<float> rArr[5];
// CHECK: OpDecorateString %sArr UserTypeGOOGLE "buffer"
Buffer<float> sArr[5];
// CHECK: OpDecorateString %tArr UserTypeGOOGLE "rwbuffer"
RWBuffer<float> tArr[5];

// CHECK: OpDecorateString %MyCBuffer UserTypeGOOGLE "cbuffer"
cbuffer MyCBuffer { float x; };

// CHECK: OpDecorateString %MyTBuffer UserTypeGOOGLE "tbuffer"
tbuffer MyTBuffer { float y; };

// CHECK: OpDecorateString %bab UserTypeGOOGLE "byteaddressbuffer"
ByteAddressBuffer bab;

// CHECK: OpDecorateString %rwbab UserTypeGOOGLE "rwbyteaddressbuffer"
RWByteAddressBuffer rwbab;

float4 main() : SV_Target{
    return 0.0.xxxx;
}

