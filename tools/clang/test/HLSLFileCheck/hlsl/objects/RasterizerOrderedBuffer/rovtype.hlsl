// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// Verify that the ROV texture type is correctly identified
// such that a struct member of that type can be assigned

RasterizerOrderedTexture2D<uint> GlobalTexture[6] : register(u0);

// Use a few different storage and declaration ways

struct ROVThings {
  RasterizerOrderedTexture2D<uint> Texture;
};


const static struct {
  RasterizerOrderedTexture2D<uint> Texture;
} thing1 = {GlobalTexture[0]};

const static ROVThings thing2 = {GlobalTexture[1]};

void main(in float4 SvPosition : SV_Position, out float4 Out : SV_Target0)
{
        // CHECK: %[[t3:[0-9a-zA-Z_]*]] = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 2
        const struct {
          RasterizerOrderedTexture2D<uint> Texture;
        } thing3 = {GlobalTexture[2]};
        // CHECK: %[[t4:[0-9a-zA-Z_]*]] = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 3
        struct {
          RasterizerOrderedTexture2D<uint> Texture;
        } thing4 = {GlobalTexture[3]};

        // CHECK: %[[t5:[0-9a-zA-Z_]*]] = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 4
        ROVThings thing5 = {GlobalTexture[4]};
        // CHECK: %[[t6:[0-9a-zA-Z_]*]] = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 5
        const ROVThings thing6 = {GlobalTexture[5]};


	Out = 0.0;

        // CHECK: %[[t1:[0-9a-zA-Z_]*]] = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0
        // CHECK: call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %[[t1]]
        // CHECK: add i32
        // CHECK: call void @dx.op.textureStore.i32(i32 67, %dx.types.Handle %[[t1]]
	thing1.Texture[uint2(SvPosition.xy)] += 1;
        // CHECK: %[[t2:[0-9a-zA-Z_]*]] = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 1
        // CHECK: call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %[[t2]]
        // CHECK: add i32
        // CHECK: call void @dx.op.textureStore.i32(i32 67, %dx.types.Handle %[[t2]]
	thing2.Texture[uint2(SvPosition.xy)] += 1;
        // CHECK: call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %[[t3]]
        // CHECK: add i32
        // CHECK: call void @dx.op.textureStore.i32(i32 67, %dx.types.Handle %[[t3]]
	thing3.Texture[uint2(SvPosition.xy)] += 1;
        // CHECK: call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %[[t4]]
        // CHECK: add i32
        // CHECK: call void @dx.op.textureStore.i32(i32 67, %dx.types.Handle %[[t4]]
	thing4.Texture[uint2(SvPosition.xy)] += 1;
        // CHECK: call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %[[t5]]
        // CHECK: add i32
        // CHECK: call void @dx.op.textureStore.i32(i32 67, %dx.types.Handle %[[t5]]
	thing5.Texture[uint2(SvPosition.xy)] += 1;
        // CHECK: call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %[[t6]]
        // CHECK: add i32
        // CHECK: call void @dx.op.textureStore.i32(i32 67, %dx.types.Handle %[[t6]]
	thing6.Texture[uint2(SvPosition.xy)] += 1;
}
