///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcvspstohitshader.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the VSPS to hit shader transform.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/Support/Global.h"
#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/HLSL/HLOperationLower.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/Support/exception.h"
#include "dxc/DXIL/DxilEntryProps.h"
#include "dxc/HLSL/DxilLinker.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilContainer.h"
#include "dxc/HLSL/DxilExportMap.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/dxcapi.h"

#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include "dxillib.h"
#include <memory>
#include <unordered_set>
#include <iterator>
#include <map>

using namespace llvm;
using namespace hlsl;

DEFINE_CROSS_PLATFORM_UUIDOF(IDxcVsPsToHitShader)

#define VSPS_DXIL_EXTERNAL_VALIDATOR 1

#if !VSPS_DXIL_EXTERNAL_VALIDATOR
// This declaration is used for the locally-linked validator.
HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);

// This internal call allows the validator to avoid having to re-deserialize
// the module. It trusts that the caller didn't make any changes and is
// kept internal because the layout of the module class may change based
// on changes across modules, or picking a different compiler version or CRT.
HRESULT RunInternalValidator(_In_ IDxcValidator *pValidator,
    _In_ llvm::Module *pModule,
    _In_ llvm::Module *pDebugModule,
    _In_ IDxcBlob *pShader, UINT32 Flags,
    _In_ IDxcOperationResult **ppResult);
#endif

namespace
{
  enum FormatCompType {
    CT_f32,
    CT_i1,
    CT_i2,
    CT_i4,
    CT_i5,
    CT_i6,
    CT_i8,
    CT_i10,
    CT_i11,
    CT_i16,
    CT_i32,
    CT_COUNT
  };

  enum FormatCvtType {
    CVT_FLOAT,
    CVT_SINT,
    CVT_SNORM,
    CVT_UINT,
    CVT_UNORM
  };

  struct FormatDesc {
    UINT format;
    FormatCompType structTypeDef[4];
    FormatCvtType  fmtCvtType;
    UINT componentCount;
  };

  static const UINT maxSupportedFormat = 115;

  static const FormatDesc formatDescs[] = {
    { 2  /*DXGI_FORMAT_R32G32B32A32_FLOAT*/, { CT_f32, CT_f32, CT_f32, CT_f32 }, CVT_FLOAT, 4 },
    { 3  /*DXGI_FORMAT_R32G32B32A32_UINT*/,  { CT_i32, CT_i32, CT_i32, CT_i32 }, CVT_UINT, 4 },
    { 4  /*DXGI_FORMAT_R32G32B32A32_SINT*/,  { CT_i32, CT_i32, CT_i32, CT_i32 }, CVT_SINT, 4 },
    { 6  /*DXGI_FORMAT_R32G32B32_FLOAT*/,    { CT_f32, CT_f32, CT_f32 }, CVT_FLOAT, 3 },
    { 7  /*DXGI_FORMAT_R32G32B32_UINT*/,     { CT_i32, CT_i32, CT_i32 }, CVT_UINT, 3 },
    { 8  /*DXGI_FORMAT_R32G32B32_SINT*/,     { CT_i32, CT_i32, CT_i32 }, CVT_SINT, 3 },
    { 10 /*DXGI_FORMAT_R16G16B16A16_FLOAT*/, { CT_i16, CT_i16, CT_i16, CT_i16 }, CVT_FLOAT, 4 },
    { 11 /*DXGI_FORMAT_R16G16B16A16_UNORM*/, { CT_i16, CT_i16, CT_i16, CT_i16 }, CVT_UNORM, 4 },
    { 12 /*DXGI_FORMAT_R16G16B16A16_UINT*/,  { CT_i16, CT_i16, CT_i16, CT_i16 }, CVT_UINT, 4 },
    { 13 /*DXGI_FORMAT_R16G16B16A16_SNORM*/, { CT_i16, CT_i16, CT_i16, CT_i16 }, CVT_SNORM, 4 },
    { 14 /*DXGI_FORMAT_R16G16B16A16_SINT*/,  { CT_i16, CT_i16, CT_i16, CT_i16 }, CVT_SINT, 4 },
    { 16 /*DXGI_FORMAT_R32G32_FLOAT*/,       { CT_f32, CT_f32 }, CVT_FLOAT, 2 },
    { 17 /*DXGI_FORMAT_R32G32_UINT*/,        { CT_i32, CT_i32 }, CVT_UINT, 2 },
    { 18 /*DXGI_FORMAT_R32G32_SINT*/,        { CT_i32, CT_i32 }, CVT_SINT, 2 },
    { 24 /*DXGI_FORMAT_R10G10B10A2_UNORM*/,  { CT_i10, CT_i10, CT_i10, CT_i2 }, CVT_UNORM, 4 },
    { 25 /*DXGI_FORMAT_R10G10B10A2_UINT*/,   { CT_i10, CT_i10, CT_i10, CT_i2 }, CVT_UINT, 4 },
    { 26 /*DXGI_FORMAT_R11G11B10_FLOAT*/,    { CT_i11, CT_i11, CT_i10 }, CVT_FLOAT, 3 },
    { 28 /*DXGI_FORMAT_R8G8B8A8_UNORM*/,     { CT_i8, CT_i8, CT_i8, CT_i8 }, CVT_UNORM, 4 },
    { 30 /*DXGI_FORMAT_R8G8B8A8_UINT*/,      { CT_i8, CT_i8, CT_i8, CT_i8 }, CVT_UINT, 4 },
    { 31 /*DXGI_FORMAT_R8G8B8A8_SNORM*/,     { CT_i8, CT_i8, CT_i8, CT_i8 }, CVT_SNORM, 4 },
    { 32 /*DXGI_FORMAT_R8G8B8A8_SINT*/,      { CT_i8, CT_i8, CT_i8, CT_i8 }, CVT_SINT, 4 },
    { 34 /*DXGI_FORMAT_R16G16_FLOAT*/,       { CT_i16, CT_i16 }, CVT_FLOAT, 2 },
    { 35 /*DXGI_FORMAT_R16G16_UNORM*/,       { CT_i16, CT_i16 }, CVT_UNORM, 2 },
    { 36 /*DXGI_FORMAT_R16G16_UINT*/,        { CT_i16, CT_i16 }, CVT_UINT, 2 },
    { 37 /*DXGI_FORMAT_R16G16_SNORM*/,       { CT_i16, CT_i16 }, CVT_SNORM, 2 },
    { 38 /*DXGI_FORMAT_R16G16_SINT*/,        { CT_i16, CT_i16 }, CVT_SINT, 2 },
    { 41 /*DXGI_FORMAT_R32_FLOAT*/,          { CT_f32 }, CVT_FLOAT, 1 },
    { 42 /*DXGI_FORMAT_R32_UINT*/,           { CT_i32 }, CVT_UINT, 1 },
    { 43 /*DXGI_FORMAT_R32_SINT*/,           { CT_i32 }, CVT_SINT, 1 },
    { 49 /*DXGI_FORMAT_R8G8_UNORM*/,         { CT_i8, CT_i8 }, CVT_UNORM, 2 },
    { 50 /*DXGI_FORMAT_R8G8_UINT*/,          { CT_i8, CT_i8 }, CVT_UINT, 2 },
    { 51 /*DXGI_FORMAT_R8G8_SNORM*/,         { CT_i8, CT_i8 }, CVT_SNORM, 2 },
    { 52 /*DXGI_FORMAT_R8G8_SINT*/,          { CT_i8, CT_i8 }, CVT_SINT, 2 },
    { 54 /*DXGI_FORMAT_R16_FLOAT*/,          { CT_i16 }, CVT_FLOAT, 1 },
    { 56 /*DXGI_FORMAT_R16_UNORM*/,          { CT_i16 }, CVT_UNORM, 1 },
    { 57 /*DXGI_FORMAT_R16_UINT*/,           { CT_i16 }, CVT_UINT, 1 },
    { 58 /*DXGI_FORMAT_R16_SNORM*/,          { CT_i16 }, CVT_SNORM, 1 },
    { 59 /*DXGI_FORMAT_R16_SINT*/,           { CT_i16 }, CVT_SINT, 1 },
    { 61 /*DXGI_FORMAT_R8_UNORM*/,           { CT_i8 }, CVT_UNORM, 1 },
    { 62 /*DXGI_FORMAT_R8_UINT*/,            { CT_i8 }, CVT_UINT, 1 },
    { 63 /*DXGI_FORMAT_R8_SNORM*/,           { CT_i8 }, CVT_SNORM, 1 },
    { 64 /*DXGI_FORMAT_R8_SINT*/,            { CT_i8 }, CVT_SINT, 1 },
    { 85 /*DXGI_FORMAT_B5G6R5_UNORM*/,       { CT_i5, CT_i6, CT_i5 }, CVT_UNORM, 3 },
    { 86 /*DXGI_FORMAT_B5G5R5A1_UNORM*/,     { CT_i5, CT_i5, CT_i5, CT_i1 }, CVT_UNORM, 4 },
    { 87 /*DXGI_FORMAT_B8G8R8A8_UNORM*/,     { CT_i8, CT_i8, CT_i8, CT_i8 }, CVT_UNORM, 4 },
    { 88 /*DXGI_FORMAT_B8G8R8X8_UNORM*/,     { CT_i8, CT_i8, CT_i8, CT_i8 }, CVT_UNORM, 3 },
    { 115/*DXGI_FORMAT_B4G4R4A4_UNORM*/,     { CT_i4, CT_i4, CT_i4, CT_i4 }, CVT_UNORM, 4 }
  };

  struct DxilDeclarations {
    Type *handleTy;
    Type *resRetI32Ty;
    Type *resRetF32Ty;
    Type *cbufRetF32Ty;
    Function *bufferLoadI32;
    Function *bufferLoadF32;
    Function *cbufferLoadI32;
    Function *cbufferLoadLegacyF32;
    Function *createHandle;
    Function *createHandleForLib;
    Function *primitiveIndex;
    Function *instanceId;
    Function *hitKind;
    Function *ignoreIntersection;
    Function *bitcastI16toF16;
  };

  struct VsInputElementDesc {
    UINT inputSlot;
    UINT alignedByteOffset;
    UINT format;
    bool inputSlotIsInstanced;
    UINT instanceDataStepRate;
    UINT inputRegister;
  };
}

static DxilDeclarations GenerateDxilDeclarations(Module *module) {
  LLVMContext &context = module->getContext();

  Type *i32Ty = Type::getInt32Ty(context);
  Type *i8Ty = Type::getInt8Ty(context);
  Type *i1Ty = Type::getInt1Ty(context);
  Type *f32Ty = Type::getFloatTy(context);
  Type *f16Ty = Type::getHalfTy(context);
  Type *i16Ty = Type::getInt16Ty(context);

  DxilDeclarations dxil = {};

  dxil.handleTy = module->getTypeByName("dx.types.Handle");

  dxil.resRetI32Ty = module->getTypeByName("dx.types.ResRet.i32");
  dxil.resRetF32Ty = module->getTypeByName("dx.types.ResRet.f32");

  if (!dxil.resRetI32Ty) {
    Type *resRetI32Types[] = { i32Ty, i32Ty, i32Ty, i32Ty, i32Ty };
    dxil.resRetI32Ty = StructType::create(resRetI32Types, "dx.types.ResRet.i32");
  }

  if (!dxil.resRetF32Ty) {
    Type *resRetF32Types[] = { f32Ty, f32Ty, f32Ty, f32Ty, i32Ty };
    dxil.resRetF32Ty = StructType::create(resRetF32Types, "dx.types.ResRet.f32");
  }

  if( !dxil.cbufRetF32Ty ) {
      Type *cbufRetF32Types[] = { f32Ty, f32Ty, f32Ty, f32Ty };
      dxil.cbufRetF32Ty = StructType::create(cbufRetF32Types, "dx.types.CBufRet.f32");
  }

  Type *bufferLoadArgs[] = { i32Ty, dxil.handleTy, i32Ty, i32Ty };
  dxil.cbufferLoadI32 = cast<Function>(module->getOrInsertFunction("dx.op.cbufferLoad.i32", FunctionType::get(i32Ty, bufferLoadArgs, false)));
  dxil.cbufferLoadI32->addFnAttr(Attribute::NoUnwind);
  dxil.cbufferLoadI32->addFnAttr(Attribute::ReadOnly);

  dxil.bufferLoadI32 = cast<Function>(module->getOrInsertFunction("dx.op.bufferLoad.i32", FunctionType::get(dxil.resRetI32Ty, bufferLoadArgs, false)));
  dxil.bufferLoadI32->addFnAttr(Attribute::NoUnwind);
  dxil.bufferLoadI32->addFnAttr(Attribute::ReadOnly);

  dxil.bufferLoadF32 = cast<Function>(module->getOrInsertFunction("dx.op.bufferLoad.f32", FunctionType::get(dxil.resRetF32Ty, bufferLoadArgs, false)));
  dxil.bufferLoadF32->addFnAttr(Attribute::NoUnwind);
  dxil.bufferLoadF32->addFnAttr(Attribute::ReadOnly);

  Type *createHandleArgs[] = { i32Ty, i8Ty, i32Ty, i32Ty, i1Ty };
  dxil.createHandle = cast<Function>(module->getOrInsertFunction("dx.op.createHandle", FunctionType::get(dxil.handleTy, createHandleArgs, false)));
  dxil.createHandle->addFnAttr(Attribute::NoUnwind);
  dxil.createHandle->addFnAttr(Attribute::ReadOnly);

  Type *primitiveIndexArgs[] = { i32Ty };
  dxil.primitiveIndex = cast<Function>(module->getOrInsertFunction("dx.op.primitiveIndex.i32", FunctionType::get(i32Ty, primitiveIndexArgs, false)));
  dxil.primitiveIndex->addFnAttr(Attribute::NoUnwind);
  dxil.primitiveIndex->addFnAttr(Attribute::ReadOnly);

  Type *instanceIdArgs[] = { i32Ty };
  dxil.instanceId = cast<Function>(module->getOrInsertFunction("dx.op.instanceID.i32", FunctionType::get(i32Ty, instanceIdArgs, false)));
  dxil.instanceId->addFnAttr(Attribute::NoUnwind);
  dxil.instanceId->addFnAttr(Attribute::ReadOnly);

  Type *hitKindArgs[] = { i32Ty };
  dxil.hitKind = cast<Function>(module->getOrInsertFunction("dx.op.hitKind.i32", FunctionType::get(i32Ty, hitKindArgs, false)));
  dxil.hitKind->addFnAttr(Attribute::NoUnwind);
  dxil.hitKind->addFnAttr(Attribute::ReadOnly);

  Type *ignoreIntersectionArgs[] = { i32Ty };
  dxil.ignoreIntersection = cast<Function>(module->getOrInsertFunction("dx.op.ignoreHit", FunctionType::get(Type::getVoidTy(context), ignoreIntersectionArgs, false)));
  dxil.ignoreIntersection->addFnAttr(Attribute::NoUnwind);
  dxil.ignoreIntersection->addFnAttr(Attribute::NoReturn);

  Type *bitcastI16toF16Args[] = { i32Ty, i16Ty };
  dxil.bitcastI16toF16 = cast<Function>(module->getOrInsertFunction("dx.op.bitcastI16toF16", FunctionType::get(f16Ty, bitcastI16toF16Args, false)));
  dxil.bitcastI16toF16->addFnAttr(Attribute::NoUnwind);
  dxil.bitcastI16toF16->addFnAttr(Attribute::ReadNone);

  return dxil;
}

static Instruction *CreateSrvHandle(const DxilDeclarations &dxil, LoadInst *load, const char *name, Instruction *insertBefore) {
  LLVMContext &context = insertBefore->getContext();
  Module *module = insertBefore->getModule();
  Type *i32Ty = Type::getInt32Ty(context);

  Type *createHandleForLibArgs[] = { i32Ty, load->getType() };
  Function *createHandleForLib = cast<Function>(module->getOrInsertFunction(Twine("dx.op.createHandleForLib.").concat(load->getType()->getStructName()).str().c_str(), FunctionType::get(dxil.handleTy, createHandleForLibArgs, false)));
  createHandleForLib->addFnAttr(Attribute::NoUnwind);
  createHandleForLib->addFnAttr(Attribute::ReadOnly);

  Value *args[] = { ConstantInt::get(i32Ty, (int)DXIL::OpCode::CreateHandleForLib), load};
  return CallInst::Create(createHandleForLib, args, name, insertBefore);
}

static Instruction *CreateRawBufferLoadF32(const DxilDeclarations &dxil, Value *handle, Value *offset, Instruction *insertBefore) {
  LLVMContext &context = insertBefore->getContext();
  Type *i32Ty = Type::getInt32Ty(context);

  Value *args[] = {
    ConstantInt::get(i32Ty, (int)DXIL::OpCode::BufferLoad),
    handle,
    offset,
    UndefValue::get(i32Ty),
  };
  return CallInst::Create(dxil.bufferLoadF32, args, "", insertBefore);
}

static Instruction *CreateRawBufferLoadI32(const DxilDeclarations &dxil, Value *handle, Value *offset, Instruction *insertBefore) {
  LLVMContext &context = insertBefore->getContext();
  Type *i32Ty = Type::getInt32Ty(context);
  
  Value *args[] = {
    ConstantInt::get(i32Ty, (int)DXIL::OpCode::BufferLoad),
    handle,
    offset,
    UndefValue::get(i32Ty),
  };
  return CallInst::Create(dxil.bufferLoadI32, args, "", insertBefore);
}

static Instruction *CreateCBufferLoadI32( const DxilDeclarations &dxil, Value *handle, Value *offset, Instruction *insertBefore ) {
    LLVMContext &context = insertBefore->getContext();
    Type *i32Ty = Type::getInt32Ty( context );

    Value *args[] = {
        ConstantInt::get( i32Ty, (int)DXIL::OpCode::CBufferLoad ),
        handle,
        offset,
        ConstantInt::get( i32Ty, 4 ), // read alignment
    };
    return CallInst::Create( dxil.cbufferLoadI32, args, "", insertBefore );
}

static Instruction *CreateGetHitKind(const DxilDeclarations &dxil, Instruction *insertBefore) {
  LLVMContext &context = insertBefore->getContext();
  Type *i32Ty = Type::getInt32Ty(context);

  Value *args[] = {
    ConstantInt::get(i32Ty, (int)DXIL::OpCode::HitKind),
  };
  return CallInst::Create(dxil.hitKind, args, "", insertBefore);
}

static Instruction *CreateIgnoreIntersection(const DxilDeclarations &dxil, BasicBlock *bb) {
  LLVMContext &context = bb->getContext();
  Type *i32Ty = Type::getInt32Ty(context);

  Value *args[] = {
    ConstantInt::get(i32Ty, (int)DXIL::OpCode::IgnoreHit),
  };
  return CallInst::Create(dxil.ignoreIntersection, args, "", bb);
}

static Instruction *CreateBitcastI16toF16(const DxilDeclarations &dxil, Value* i16Value, Instruction *insertBefore) {
  LLVMContext &context = insertBefore->getContext();
  Type *i32Ty = Type::getInt32Ty(context);

  Value *args[] = {
    ConstantInt::get(i32Ty, (int)DXIL::OpCode::BitcastI16toF16),
    i16Value,
  };
  return CallInst::Create(dxil.bitcastI16toF16, args, "", insertBefore);
}

static void ImportFunctionInto(ValueToValueMapTy &valueMap, const DxilDeclarations &dxil, Function *inputFunction, Function *outputFunction) {
  struct DxilValueMapTypeRemapper : public ValueMapTypeRemapper {
    const DxilDeclarations *dxil;

    virtual Type *remapType(Type *type) {
      if (!type->isStructTy())
        return type;

      if (type->getStructName().startswith("dx.types.Handle"))
        return dxil->handleTy;

      if (type->getStructName().startswith("dx.types.ResRet.f32"))
        return dxil->resRetF32Ty;

      if (type->getStructName().startswith("dx.types.ResRet.i32"))
        return dxil->resRetI32Ty;

      return type;
    }
  };

  DxilValueMapTypeRemapper typeRemapper;
  typeRemapper.dxil = &dxil;

  Module *inputModule = inputFunction->getParent();
  Module *outputModule = outputFunction->getParent();

  // Update valueMap with function declarations.
  for (Function &func : *inputModule) {
    if (func.isDeclaration()) {
      Function *newFunc = outputModule->getFunction(func.getName());

      if (!newFunc) {
        FunctionType *functionType = func.getFunctionType();

        SmallVector<Type*, 8> arguments;
        arguments.resize(functionType->getNumParams());

        for (int i = 0; i < (int)arguments.size(); ++i)
          arguments[i] = typeRemapper.remapType(functionType->getParamType(i));

        Type *returnType = typeRemapper.remapType(functionType->getReturnType());

        functionType = FunctionType::get(returnType, arguments, false);

        newFunc = cast<Function>(outputModule->getOrInsertFunction(func.getName(), functionType));
        newFunc->setLinkage(func.getLinkage());
        newFunc->setAttributes(func.getAttributes());
      }

      DXASSERT_NOMSG(func.getFunctionType()->getNumParams() == newFunc->getFunctionType()->getNumParams());
      valueMap[&func] = newFunc;
    }
  }

  // Clone.
  SmallVector<ReturnInst*, 8> returns;
  CloneFunctionInto(outputFunction, inputFunction, valueMap, true, returns, "", 0, &typeRemapper);
  valueMap[inputFunction] = outputFunction;
}

static StructType *CreateVSOutType(LLVMContext &context, const std::vector<std::unique_ptr<DxilSignatureElement>> &vsOutputSignatures) {
  Type *i32Ty = Type::getInt32Ty(context);
  Type *f32Ty = Type::getFloatTy(context);

  SmallVector<Type*, 8> vsOutMembers;

  for (size_t i = 0; i < vsOutputSignatures.size(); ++i) {
    const DxilSignatureElement &signature = *vsOutputSignatures[i];
    Type *componentType = nullptr;

    switch (signature.GetCompType().GetKind()) {
    case DXIL::ComponentType::I1:
    case DXIL::ComponentType::I16:
    case DXIL::ComponentType::U16:
    case DXIL::ComponentType::I32:
    case DXIL::ComponentType::U32:
    case DXIL::ComponentType::I64:
    case DXIL::ComponentType::U64:
      componentType = i32Ty;
      break;
    case DXIL::ComponentType::F16:
    case DXIL::ComponentType::F32:
    case DXIL::ComponentType::F64:
    case DXIL::ComponentType::SNormF16:
    case DXIL::ComponentType::UNormF16:
    case DXIL::ComponentType::SNormF32:
    case DXIL::ComponentType::UNormF32:
    case DXIL::ComponentType::SNormF64:
    case DXIL::ComponentType::UNormF64:
      componentType = f32Ty;
      break;
    default:
      break;
    }

    int count = signature.GetRows() * signature.GetCols();
    vsOutMembers.push_back(VectorType::get(componentType, count));
  }

  return StructType::create(vsOutMembers, "struct.vsps.VSOut");
}

static Function *GenerateVSOutAdd(Module *module, Type *vsOutTy, const std::vector<std::unique_ptr<DxilSignatureElement>> &vsOutputSignatures) {
  LLVMContext &context = module->getContext();

  Type *vsOutAddArgs[] = { vsOutTy, vsOutTy };
  Function *vsOutAdd = cast<Function>(module->getOrInsertFunction("VSOutAdd", FunctionType::get(vsOutTy, vsOutAddArgs, false)));
  vsOutAdd->addFnAttr(Attribute::AlwaysInline);

  BasicBlock *body = BasicBlock::Create(context, "", vsOutAdd);
  auto args = vsOutAdd->getArgumentList().begin();

  Value *result = UndefValue::get(vsOutTy);
  Instruction *insertBefore = ReturnInst::Create(context, result, body);

  Value *aStruct = args++;
  Value *bStruct = args;

  for (size_t i = 0; i < vsOutputSignatures.size(); ++i) {
    const DxilSignatureElement &signature = *vsOutputSignatures[i];

    if (!signature.GetInterpolationMode()->IsUndefined() && aStruct->getType()->getStructElementType(i)->getVectorElementType()->isFloatingPointTy()) {
      Value *a = ExtractValueInst::Create(aStruct, i, "", insertBefore);
      Value *b = ExtractValueInst::Create(bStruct, i, "", insertBefore);
      Value *c = BinaryOperator::Create(Instruction::FAdd, a, b, "", insertBefore);
      result = InsertValueInst::Create(result, c, i, "", insertBefore);
    } else {
      Value *a = ExtractValueInst::Create(aStruct, i, "", insertBefore);
      result = InsertValueInst::Create(result, a, i, "", insertBefore);
    }
  }

  ReturnInst::Create(context, result, insertBefore);
  insertBefore->eraseFromParent();

  return vsOutAdd;
}

Function *GenerateVSOutMul(Module *module, Type *vsOutTy, const std::vector<std::unique_ptr<DxilSignatureElement>> &vsOutputSignatures) {
  LLVMContext &context = module->getContext();
  Type *i32Ty = Type::getInt32Ty(context);
  Type *f32Ty = Type::getFloatTy(context);

  Type *vsOutMulArgs[] = { vsOutTy, f32Ty };
  Function *vsOutMul = cast<Function>(module->getOrInsertFunction("VSOutMul", FunctionType::get(vsOutTy, vsOutMulArgs, false)));
  vsOutMul->addFnAttr(Attribute::AlwaysInline);

  BasicBlock *body = BasicBlock::Create(context, "", vsOutMul);
  auto args = vsOutMul->getArgumentList().begin();

  Value *result = UndefValue::get(vsOutTy);
  Instruction *insertBefore = ReturnInst::Create(context, result, body);

  Value *aStruct = args++;
  Value *bFloat = args;

  for (size_t i = 0; i < vsOutputSignatures.size(); ++i) {
    const DxilSignatureElement &signature = *vsOutputSignatures[i];

    if (!signature.GetInterpolationMode()->IsUndefined() && aStruct->getType()->getStructElementType(i)->getVectorElementType()->isFloatingPointTy()) {
      Value *a = ExtractValueInst::Create(aStruct, (unsigned)i, "", insertBefore);
      Value *c = UndefValue::get(a->getType());

      for (unsigned j = 0; j < a->getType()->getVectorNumElements(); ++j) {
        Value *value = ExtractElementInst::Create(a, ConstantInt::get(i32Ty, j), "", insertBefore);
        value = BinaryOperator::Create(Instruction::FMul, value, bFloat, "", insertBefore);
        c = InsertElementInst::Create(c, value, ConstantInt::get(i32Ty, j), "", insertBefore);
      }

      result = InsertValueInst::Create(result, c, i, "", insertBefore);
    } else {
      Value *a = ExtractValueInst::Create(aStruct, i, "", insertBefore);
      result = InsertValueInst::Create(result, a, i, "", insertBefore);
    }
  }

  ReturnInst::Create(context, result, insertBefore);
  insertBefore->eraseFromParent();

  return vsOutMul;
}

static UINT SemanticToRegisterNumber(LPCSTR semanticName, UINT semanticIndex, const std::vector<std::unique_ptr<DxilSignatureElement>> &signatureElements) {
  for (size_t i = 0; i < signatureElements.size(); ++i) {
    const DxilSignatureElement &element = *signatureElements[i];

    if (StringRef(element.GetSemanticName()).compare_lower(semanticName) == 0) {
      const std::vector<unsigned>& semanticIndices = element.GetSemanticIndexVec();

      for (size_t j = 0; j < semanticIndices.size(); ++j) {
        if (semanticIndex == semanticIndices[j])
          return element.GetStartRow() + (int)j;
      }
    }
  }

  return UINT_MAX;
}

static void DxcInputLayoutToVsInputElementLayout(DxcInputLayoutDesc inputLayout, SmallVectorImpl<VsInputElementDesc> &outInputLayout, const std::vector<std::unique_ptr<DxilSignatureElement>> &inputSignatureElts) {
  for (UINT i = 0; i < inputLayout.NumElements; ++i) {
    const DxcInputElementDesc &input = inputLayout.pInputElementDescs[i];

    UINT registerIdx = SemanticToRegisterNumber(input.SemanticName, input.SemanticIndex, inputSignatureElts);

    if (registerIdx != UINT_MAX) {
      VsInputElementDesc output = {};
      output.inputSlot            = input.InputSlot;
      output.alignedByteOffset    = input.AlignedByteOffset;
      output.format               = input.DxgiFormat;
      output.inputSlotIsInstanced = input.InputSlotClass == 1/*D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA*/;
      output.instanceDataStepRate = input.InstanceDataStepRate;
      output.inputRegister        = registerIdx;
      outInputLayout.push_back(output);
    }
  }
}

static void AppendCallsToFunction(Function *callee, Function *caller, std::vector<CallInst*>& calls) {
  for (User *user : callee->users()) {
    CallInst *call = dyn_cast<CallInst>(user);

    if (!call)
      continue;

    if (caller == nullptr || call->getParent()->getParent() == caller)
        calls.push_back(call);
  }
}

static int SignatureElementIndexFromId(uint32_t id, const DxilSignature &signature) {
  const std::vector<std::unique_ptr<DxilSignatureElement> > &elements = signature.GetElements();

  for (size_t i = 0; i < elements.size(); ++i) {
    if (elements[i]->GetID() == id)
      return (int)i;
  }

  return -1;
}

static void MergeResources(
  std::vector<std::unique_ptr<DxilResource>> &srvResources,
  std::vector<std::unique_ptr<DxilResource>> &uavResources,
  std::vector<std::unique_ptr<DxilCBuffer>> &cbvResources,
  std::vector<std::unique_ptr<DxilSampler>> &samplerResources,
  DxilModule *DM, DxilModule *vsDxil, DxilModule *psDxil, Function *vsMain, Function *psMain,
  UINT vsRegisterSpaceAddend, UINT psRegisterSpaceAddend) {

  static const uint64_t RESMAP_VS = 0;
  static const uint64_t RESMAP_PS = (uint64_t)1 << 63;
  static const uint64_t RESMAP_KIND_OFFSET = 48;
  static const uint64_t RESMAP_SRV = (uint64_t)0 << RESMAP_KIND_OFFSET;
  static const uint64_t RESMAP_UAV = (uint64_t)1 << RESMAP_KIND_OFFSET;
  static const uint64_t RESMAP_CBV = (uint64_t)2 << RESMAP_KIND_OFFSET;
  static const uint64_t RESMAP_SAMPLER = (uint64_t)3 << RESMAP_KIND_OFFSET;

  for (const std::unique_ptr<DxilResource> &resource : DM->GetSRVs()) {
    std::unique_ptr<DxilResource> clone(new DxilResource(*resource));
    clone->SetID((unsigned)srvResources.size());
    clone->SetSpaceID( clone->GetSpaceID() );
    srvResources.push_back(std::move(clone));
  }

  for (const std::unique_ptr<DxilResource> &resource : DM->GetUAVs()) {
    std::unique_ptr<DxilResource> clone(new DxilResource(*resource));
    clone->SetID((unsigned)uavResources.size());
    clone->SetSpaceID( clone->GetSpaceID() );
    uavResources.push_back(std::move(clone));
  }

  for (const std::unique_ptr<DxilCBuffer> &resource : DM->GetCBuffers()) {
    std::unique_ptr<DxilCBuffer> clone(new DxilCBuffer(*resource));
    clone->SetID((unsigned)cbvResources.size());
    clone->SetSpaceID( clone->GetSpaceID() );
    cbvResources.push_back(std::move(clone));
  }

  for (const std::unique_ptr<DxilSampler> &resource : DM->GetSamplers()) {
    std::unique_ptr<DxilSampler> clone(new DxilSampler(*resource));
    clone->SetID((unsigned)samplerResources.size());
    clone->SetSpaceID( clone->GetSpaceID() );
    samplerResources.push_back(std::move(clone));
  }
}

static Function *GenerateHitMain(StringRef name, int indexByteSize, const DxilDeclarations &dxil, Type *payloadTy, Type *attributeTy, Function *vsMain, Function *psMain,
    Function *vsOutMul, Function *vsOutAdd, UINT iaRegisterSpace, std::vector<std::unique_ptr<DxilResource>> &srvResources, std::vector<std::unique_ptr<DxilCBuffer>> &cbvResources) {
  LLVMContext &context = vsMain->getContext();

  Type *i32Ty = Type::getInt32Ty(context);
  Type *f32Ty = Type::getFloatTy(context);

  Module *module = vsMain->getParent();

  Type *hitMainArgs[] = { payloadTy->getPointerTo(), attributeTy->getPointerTo() };
  Function *hitMain = cast<Function>(module->getOrInsertFunction(name, FunctionType::get(Type::getVoidTy(context), hitMainArgs, false)));

  auto args = hitMain->getArgumentList().begin();

  Value *payloadPtr = args++;
  Value *attributes = args;

  BasicBlock *body = BasicBlock::Create(context, "", hitMain);
  Instruction *insertBefore = ReturnInst::Create(context, body);

  Value *instanceIdArgs[] = { ConstantInt::get(i32Ty, (int)DXIL::OpCode::InstanceID) };
  Value *instanceId = CallInst::Create(dxil.instanceId, instanceIdArgs, "vsps.instanceId", insertBefore);

  Value *primitiveIdArgs[] = { ConstantInt::get(i32Ty, (int)DXIL::OpCode::PrimitiveIndex) };
  Value *primitiveId = CallInst::Create(dxil.primitiveIndex, primitiveIdArgs, "vsps.primitiveId", insertBefore);

  Value *vIndices[] = { ConstantInt::get(i32Ty, 0), ConstantInt::get(i32Ty, 0) };
  Value *wIndices[] = { ConstantInt::get(i32Ty, 0), ConstantInt::get(i32Ty, 1) };

  Value *v = new LoadInst(GetElementPtrInst::CreateInBounds(attributes, vIndices, "", insertBefore), "vsps.u", insertBefore);
  Value *w = new LoadInst(GetElementPtrInst::CreateInBounds(attributes, wIndices, "", insertBefore), "vsps.v", insertBefore);
  Value *u = BinaryOperator::CreateFSub(BinaryOperator::CreateFSub(ConstantFP::get(f32Ty, 1.0f), v, "", insertBefore), w, "vsps.w", insertBefore);

  Value *indexBufferHandle = nullptr;
  Value *ibByteOffCbvHandle = nullptr;

  if (indexByteSize != 0) {
    unsigned indexBufferSrvId = (unsigned)srvResources.size();
    unsigned ibByteOffCbvId = (unsigned)cbvResources.size();
    LoadInst *load;

    if (indexByteSize != 0) {
      Type *indexTy = StructType::create(i32Ty, "struct.ByteAddressBuffer.vsps.IndexBuffer");

      std::unique_ptr<DxilResource> srv(new DxilResource());

      srv->SetID(indexBufferSrvId);
      srv->SetGlobalSymbol(UndefValue::get(indexTy->getPointerTo()));
      srv->SetGlobalName("vsps.indexBuffer");
      srv->SetSpaceID(iaRegisterSpace);
      srv->SetLowerBound(0);
      srv->SetRangeSize(1);
      srv->SetKind(DXIL::ResourceKind::RawBuffer);
      srv->SetSampleCount(0);
      srv->SetRW(false);

      auto *GV = new llvm::GlobalVariable(
          *module, indexTy, false,
          llvm::GlobalValue::ExternalLinkage, nullptr, srv->GetGlobalName(), nullptr,
          llvm::GlobalVariable::NotThreadLocal, 0);
      srv->SetGlobalSymbol( GV );
      load = new LoadInst( srv->GetGlobalSymbol(), srv->GetGlobalName(), insertBefore );

      srvResources.push_back(std::move(srv));
      indexBufferHandle = CreateSrvHandle(dxil, load, "vsps.indexBufferHandle", insertBefore);

      Type *ibByteOffTy = StructType::create(i32Ty, "cbuffer.vsps.IbByteOffsetCbuffer");

      std::unique_ptr<DxilCBuffer> cbv(new DxilCBuffer());

      cbv->SetID(ibByteOffCbvId);
      cbv->SetGlobalSymbol(UndefValue::get(ibByteOffTy->getPointerTo()));
      cbv->SetGlobalName("vsps.ibByteOffsetCbuffer");
      cbv->SetSpaceID(iaRegisterSpace);
      cbv->SetLowerBound(0);
      cbv->SetRangeSize(1);
      cbv->SetKind(DXIL::ResourceKind::CBuffer);
      cbv->SetSize(sizeof(unsigned int));

      auto *GVcbv = new llvm::GlobalVariable(
          *module, indexTy, false,
          llvm::GlobalValue::ExternalLinkage, nullptr, cbv->GetGlobalName(), nullptr,
          llvm::GlobalVariable::NotThreadLocal, 0);
      cbv->SetGlobalSymbol( GVcbv );
      load = new LoadInst( cbv->GetGlobalSymbol(), cbv->GetGlobalName(), insertBefore );

      cbvResources.push_back(std::move(cbv));
      ibByteOffCbvHandle = CreateSrvHandle(dxil, load, "vsps.ibByteOffsetCbufferHandle", insertBefore);
    }
  }

  Value *index0, *index1, *index2;

  if (indexByteSize == 4) {
    Value *offset = BinaryOperator::Create(Instruction::Mul, primitiveId, ConstantInt::get(i32Ty, 12), "", insertBefore);
    Value *indices = CreateRawBufferLoadI32(dxil, indexBufferHandle, offset, insertBefore);

    index0 = ExtractValueInst::Create(indices, 0, "vsps.index0", insertBefore);
    index1 = ExtractValueInst::Create(indices, 1, "vsps.index1", insertBefore);
    index2 = ExtractValueInst::Create(indices, 2, "vsps.index2", insertBefore);
  }
  else if (indexByteSize == 2) {
    Value *ibByteOffset = CreateCBufferLoadI32(dxil, ibByteOffCbvHandle, ConstantInt::get(i32Ty, 0), insertBefore);
    Value *offset = BinaryOperator::Create(Instruction::Mul, primitiveId, ConstantInt::get(i32Ty, 6), "", insertBefore);
    offset = BinaryOperator::Create(Instruction::Add, offset, ibByteOffset, "", insertBefore);
    Value *offset4Baligned = BinaryOperator::Create(Instruction::And, offset, ConstantInt::get(i32Ty, ~3), "", insertBefore);

    Value *indices = CreateRawBufferLoadI32(dxil, indexBufferHandle, offset4Baligned, insertBefore);

    Value *part01 = ExtractValueInst::Create(indices, 0, "vsps.index01", insertBefore);
    Value *part23 = ExtractValueInst::Create(indices, 1, "vsps.index23", insertBefore);

    Value *mask = BinaryOperator::Create(Instruction::And, offset, ConstantInt::get(i32Ty, 0x2), "", insertBefore);
    mask = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_EQ, mask, ConstantInt::get(i32Ty, 0), "", insertBefore);

    Value *part0 = BinaryOperator::Create(Instruction::And, part01, ConstantInt::get(i32Ty, 0xffff), "", insertBefore);
    Value *part1 = BinaryOperator::Create(Instruction::LShr, part01, ConstantInt::get(i32Ty, 16), "", insertBefore);
    Value *part2 = BinaryOperator::Create(Instruction::And, part23, ConstantInt::get(i32Ty, 0xffff), "", insertBefore);
    Value *part3 = BinaryOperator::Create(Instruction::LShr, part23, ConstantInt::get(i32Ty, 16), "", insertBefore);

    index0 = SelectInst::Create(mask, part0, part1, "vsps.index0", insertBefore);
    index1 = SelectInst::Create(mask, part1, part2, "vsps.index1", insertBefore);
    index2 = SelectInst::Create(mask, part2, part3, "vsps.index2", insertBefore);
  }
  else {
    DXASSERT_NOMSG(indexByteSize == 0);

    index0 = BinaryOperator::Create(Instruction::Mul, primitiveId, ConstantInt::get(i32Ty, 3), "", insertBefore);
    index1 = BinaryOperator::Create(Instruction::Add, index0, ConstantInt::get(i32Ty, 1), "", insertBefore);
    index2 = BinaryOperator::Create(Instruction::Add, index0, ConstantInt::get(i32Ty, 2), "", insertBefore);
  }

  Value *v0;
  {
    Value *args[] = { instanceId, index0 };
    v0 = CallInst::Create(vsMain, args, "v0", insertBefore);
  }
  {
    Value *args[] = { v0, u };
    v0 = CallInst::Create(vsOutMul, args, "", insertBefore);
  }

  Value *v1;
  {
    Value *args[] = { instanceId, index1 };
    v1 = CallInst::Create(vsMain, args, "v1", insertBefore);
  }
  {
    Value *args[] = { v1, v };
    v1 = CallInst::Create(vsOutMul, args, "", insertBefore);
  }

  Value *v01;
  {
    Value *args[] = { v0, v1 };
    v01 = CallInst::Create(vsOutAdd, args, "v01", insertBefore);
  }

  Value *v2;
  {
    Value *args[] = { instanceId, index2 };
    v2 = CallInst::Create(vsMain, args, "v2", insertBefore);
  }
  {
    Value *args[] = { v2, w };
    v2 = CallInst::Create(vsOutMul, args, "", insertBefore);
  }

  Value *v012;
  {
    Value *args[] = { v01, v2 };
    v012 = CallInst::Create(vsOutAdd, args, "v012", insertBefore);
  }

  {
    Value *args[] = { payloadPtr, v012 };
    CallInst::Create(psMain, args, "", insertBefore);
  }

  return hitMain;
}

static HRESULT ConvertVSMainInputOutput(const DxilDeclarations &dxil, Function *vsMain, Function *vsOriginalFunction, DxilModule *vsDxil, const SmallVectorImpl<VsInputElementDesc> &inputLayout, unsigned indexByteSize, const UINT* vertexBufferStrides, UINT iaRegisterSpace, std::vector<std::unique_ptr<DxilResource>> &srvResources) {
  LLVMContext &context = vsMain->getContext();

  Type *i32Ty = Type::getInt32Ty(context);
  Type *vsOutTy = vsMain->getReturnType();

  // Determine the number of vertex slots.
  size_t vertexSlotCount = 0;

  for (size_t i = 0; i < inputLayout.size(); ++i) {
    if (vertexSlotCount <= (size_t)inputLayout[i].inputSlot)
      vertexSlotCount = (size_t)inputLayout[i].inputSlot + 1;
  }

  // Initialize input format types.
  Type *scalarTypes[CT_COUNT];
  scalarTypes[CT_f32] = Type::getFloatTy(context);
  scalarTypes[CT_i1]  = Type::getInt1Ty(context);
  scalarTypes[CT_i2]  = IntegerType::get(context, 2);
  scalarTypes[CT_i4]  = IntegerType::get(context, 4);
  scalarTypes[CT_i5]  = IntegerType::get(context, 5);
  scalarTypes[CT_i6]  = IntegerType::get(context, 6);
  scalarTypes[CT_i8]  = Type::getInt8Ty(context);
  scalarTypes[CT_i10] = IntegerType::get(context, 10);
  scalarTypes[CT_i11] = IntegerType::get(context, 11);
  scalarTypes[CT_i16] = Type::getInt16Ty(context);
  scalarTypes[CT_i32] = Type::getInt32Ty(context);

  int scalarBitSizes[CT_COUNT];
  scalarBitSizes[CT_f32] = 32;
  scalarBitSizes[CT_i1]  = 1;
  scalarBitSizes[CT_i2]  = 2;
  scalarBitSizes[CT_i4]  = 4;
  scalarBitSizes[CT_i5]  = 5;
  scalarBitSizes[CT_i6]  = 6;
  scalarBitSizes[CT_i8]  = 8;
  scalarBitSizes[CT_i10] = 10;
  scalarBitSizes[CT_i11] = 11;
  scalarBitSizes[CT_i16] = 16;
  scalarBitSizes[CT_i32] = 32;

  // Initialize format map.
  UINT dxgiFormatToFormatIdx[maxSupportedFormat+1] = {};
  memset(dxgiFormatToFormatIdx, 0xff, sizeof(dxgiFormatToFormatIdx));

  for (size_t i = 0; i < sizeof(formatDescs)/sizeof(formatDescs[0]); ++i) {
    assert(formatDescs[i].format <= maxSupportedFormat);
    dxgiFormatToFormatIdx[formatDescs[i].format] = (UINT)i;
  }

  // Generate vertex buffer loads.
  SmallVector<Instruction*, 8> vertexBufferLoads;
  SmallVector<size_t, 8> vertexBufferSlotFirstLoad;
  {
    auto arguments = vsMain->getArgumentList().begin();
    Value *vsInstanceIndex = arguments++;
    Value *vsVertexIndex = arguments;

    Instruction *insertBefore = vsMain->begin()->begin();

    for (size_t i = 0; i < vertexSlotCount; ++i) {
      size_t sizeInFloats = 0;
      size_t resourceOffset = indexByteSize == 0 ? 0 : 1;

      for (size_t j = 0; j < inputLayout.size(); ++j) {
        if (inputLayout[j].inputSlot != i)
          continue;

        UINT format = inputLayout[j].format;

        if (format > maxSupportedFormat) {
          // Unsupported format.
          return DXC_E_NOT_SUPPORTED;
        }

        UINT formatIdx = dxgiFormatToFormatIdx[format];

        if (formatIdx >= sizeof(formatDescs)/sizeof(formatDescs[0])) {
          // Unsupported format.
          return DXC_E_NOT_SUPPORTED;
        }

        const FormatDesc &formatDesc = formatDescs[formatIdx];
        unsigned sizeInBits = 0;

        for (UINT k = 0; k < formatDesc.componentCount; ++k)
          sizeInBits += scalarBitSizes[formatDesc.structTypeDef[k]];

        unsigned endInFloats = (inputLayout[j].alignedByteOffset + sizeInBits/8 + sizeof(float)-1) / sizeof(float);

        if (sizeInFloats < endInFloats)
          sizeInFloats = endInFloats;
      }

      unsigned uniqueId = (unsigned)srvResources.size();

      Type *vertexTy = StructType::create(i32Ty, Twine("struct.ByteAddressBuffer.vsps.VertexBuffer").concat(Twine(i)).str());

      std::unique_ptr<DxilResource> srv(new DxilResource());

      srv->SetID(uniqueId);
      srv->SetGlobalSymbol(UndefValue::get(vertexTy->getPointerTo()));
      srv->SetGlobalName(Twine("vsps.vertexBuffer").concat(Twine(i)).str());
      srv->SetSpaceID(iaRegisterSpace);
      srv->SetLowerBound((unsigned)(i + resourceOffset));
      srv->SetRangeSize(1);
      srv->SetKind(DXIL::ResourceKind::RawBuffer);
      srv->SetSampleCount(0);
      srv->SetRW(false);

      auto *GV = new llvm::GlobalVariable(
          *(vsDxil->GetModule()), vertexTy, false,
          llvm::GlobalValue::ExternalLinkage, nullptr, srv->GetGlobalName(), nullptr,
          llvm::GlobalVariable::NotThreadLocal, 0);
      srv->SetGlobalSymbol( GV );

      // Create handle.
      LoadInst *load = new LoadInst( srv->GetGlobalSymbol(), srv->GetGlobalName(), insertBefore );
      Value *vertexBufferHandle = CreateSrvHandle(dxil, load, Twine("vsps.vertexBuffer").concat(Twine(i)).concat(Twine("Handle")).str().c_str(), insertBefore);

      srvResources.push_back(std::move(srv));

      vertexBufferSlotFirstLoad.push_back(vertexBufferLoads.size());

      Value *indexIntoBuffer = nullptr;
      VsInputElementDesc vertexElement = {};

      for (size_t j = 0; j < inputLayout.size(); ++j) {
        if ((size_t)inputLayout[j].inputSlot == i) {
          vertexElement = inputLayout[j];
          break;
        }
      }

      if (vertexElement.inputSlotIsInstanced) {
        // Instanced vertex attributes.
        if (vertexElement.instanceDataStepRate == 0) {
          indexIntoBuffer = ConstantInt::get(i32Ty, 0);
        }
        else if (isPowerOf2_32(vertexElement.instanceDataStepRate)) {
          int log2InstDataStepRate = Log2_32(vertexElement.instanceDataStepRate);
          indexIntoBuffer = BinaryOperator::CreateLShr(vsInstanceIndex, ConstantInt::get(i32Ty, log2InstDataStepRate), "", insertBefore);
        }
        else {
          indexIntoBuffer = BinaryOperator::CreateUDiv(vsInstanceIndex, ConstantInt::get(i32Ty, vertexElement.instanceDataStepRate), "", insertBefore);
        }
      }
      else {
        // Regular vertex attributes.
        indexIntoBuffer = vsVertexIndex;
      }

      // Loading everything up to vertex buffer stride as f32. Unused loads will be removed later.
      Value *vertexOffset = BinaryOperator::Create(Instruction::Mul, indexIntoBuffer, ConstantInt::get(i32Ty, vertexBufferStrides[i]), "", insertBefore);

      for (size_t j = 0; j < sizeInFloats; j += 4) {
        Value *offset = BinaryOperator::Create(Instruction::Add, vertexOffset, ConstantInt::get(i32Ty, j*sizeof(float)), "", insertBefore);
        vertexBufferLoads.push_back(CreateRawBufferLoadF32(dxil, vertexBufferHandle, offset, insertBefore));
      }
    }
  }

  // Replace dx.op.loadInput/storeOutput in vsMain and patch returns.
  {
    auto arguments = vsMain->getArgumentList().begin();
    Value *vsInstanceIndex = arguments++;
    Value *vsVertexIndex = arguments;

    // Allocate VSOut on the stack.
    AllocaInst *vsOut = new AllocaInst(vsOutTy, "vsps.vsOut", vsMain->begin()->begin());

    // Find instructions to patch.
    std::vector<CallInst*> loadInputs;
    std::vector<CallInst*> storeOutputs;
    std::vector<TerminatorInst*> returns;

    for (Function &func : *vsMain->getParent()) {
      if (func.getName().startswith("dx.op.loadInput."))
        AppendCallsToFunction(&func, vsMain, loadInputs);
      else if (func.getName().startswith("dx.op.storeOutput."))
        AppendCallsToFunction(&func, vsMain, storeOutputs);
    }

    for (Function::iterator i = vsMain->begin(), end = vsMain->end(); i != end; ++i) {
      if (TerminatorInst *terminator = i->getTerminator())
        if (ReturnInst *ret = dyn_cast<ReturnInst>(terminator))
          returns.push_back(ret);
    }

    // Build mapping from vector register index to input layout element.
    SmallVector<UINT, 8> vecRegIdxToInputLayoutElement;

    for (size_t i = 0; i < inputLayout.size(); ++i) {
      UINT index = inputLayout[i].inputRegister;

      if (index >= vecRegIdxToInputLayoutElement.size())
        vecRegIdxToInputLayoutElement.resize(index+1, UINT_MAX);

      vecRegIdxToInputLayoutElement[index] = i;
    }

    DxilEntrySignature vsEntrySig = vsDxil->GetDxilEntrySignature(vsOriginalFunction);

    // Patch loadInput.
    for (CallInst *call : loadInputs) {
      int signatureId = (int)cast<ConstantInt>(call->getArgOperand(1))->getSExtValue();
      int signatureIdx = SignatureElementIndexFromId(signatureId, vsEntrySig.InputSignature);
      DXASSERT_NOMSG(signatureIdx >= 0);

      const DxilSignatureElement &signature = vsEntrySig.InputSignature.GetElement(signatureIdx);

      if (signature.GetSemantic()->GetKind() == DXIL::SemanticKind::VertexID) {
        call->replaceAllUsesWith(vsVertexIndex);
        call->eraseFromParent();
        continue;
      }
      else if (signature.GetSemantic()->GetKind() == DXIL::SemanticKind::InstanceID) {
        call->replaceAllUsesWith(vsInstanceIndex);
        call->eraseFromParent();
        continue;
      }
      else if (signature.GetSemantic()->GetKind() == DXIL::SemanticKind::IsFrontFace) {
        Value *hitKind = CreateGetHitKind(dxil, call);

        Type *i1Ty = Type::getInt1Ty(context);
        Value *isFrontFacei1 = CastInst::Create(Instruction::Trunc, hitKind, i1Ty, "", call);
        isFrontFacei1 = BinaryOperator::Create(Instruction::Xor, isFrontFacei1, ConstantInt::get(i1Ty, 1), "", call);

        Value *isFrontFace = CastInst::Create(Instruction::ZExt, isFrontFacei1, call->getType(), "", call);
        call->replaceAllUsesWith(isFrontFace);
        call->eraseFromParent();
        continue;
      }

      Value *rowIdx = UndefValue::get(i32Ty);

      if (ConstantInt *rowOffCnst = dyn_cast<ConstantInt>(call->getArgOperand(2))) {
        int rowIdxVal = (int)rowOffCnst->getZExtValue();

        if (rowIdxVal != 0)
          rowIdx = ConstantInt::get(i32Ty, rowIdxVal);
      }
      else {
        rowIdx = call->getArgOperand(2);
      }

      bool hasConstantIndex = isa<ConstantInt>(rowIdx);

      if (!isa<UndefValue>(rowIdx) && !hasConstantIndex) {
        // Dynamically indexed signature not supported.
        return DXC_E_NOT_SUPPORTED;
      }

      int vecRegIdx = signature.GetStartRow();
      if (hasConstantIndex)
        vecRegIdx += (int)cast<ConstantInt>(rowIdx)->getSExtValue();
      DXASSERT_NOMSG((size_t)vecRegIdx < vecRegIdxToInputLayoutElement.size());

      UINT vtxEltIdx = vecRegIdxToInputLayoutElement[vecRegIdx];
      DXASSERT_NOMSG(vtxEltIdx < (UINT)inputLayout.size());
      const VsInputElementDesc &vertexElement = inputLayout[vtxEltIdx];

      if (vertexElement.format > maxSupportedFormat) {
        // Unsupported format.
        return DXC_E_NOT_SUPPORTED;
      }

      if (vertexElement.format == 26/*DXGI_FORMAT_R11G11B10_FLOAT*/) {
        // Unsupported format.
        return DXC_E_NOT_SUPPORTED;
      }

      // Lookup format.
      UINT formatIdx = dxgiFormatToFormatIdx[vertexElement.format];

      if (formatIdx >= sizeof(formatDescs)/sizeof(formatDescs[0])) {
        // Unsupported format.
        return DXC_E_NOT_SUPPORTED;
      }

      const FormatDesc &formatDesc = formatDescs[formatIdx];

      // Calculate column.
      int column = signature.GetStartCol() + (int)cast<ConstantInt>(call->getArgOperand(3))->getZExtValue();

      Value *replacement = nullptr;

      if (column >= (int)formatDesc.componentCount) {
        // Default value.
        if (call->getType()->isFloatingPointTy())
          replacement = ConstantFP::get(call->getType(), (column == 3) ? 1.0f : 0.0f);
        else
          replacement = ConstantInt::get(call->getType(), (column == 3) ? 1 : 0);
      }
      else {
        // Read component from a loaded word.
        int vertexSlot = vertexElement.inputSlot;
        int bitOffset = vertexElement.alignedByteOffset * 8;

        for (int i = 0; i < column; ++i)
          bitOffset += scalarBitSizes[formatDesc.structTypeDef[i]];

        int bitSize = scalarBitSizes[formatDesc.structTypeDef[column]];
        int startWord = bitOffset / 32;

        DXASSERT_NOMSG((bitOffset + bitSize + 31) / 32 - startWord == 1);
        {
          int row = startWord / 4;
          int column = startWord % 4;
          replacement = ExtractValueInst::Create(vertexBufferLoads[vertexBufferSlotFirstLoad[vertexSlot] + row], column, "", call);
        }

        if (formatDesc.structTypeDef[column] != CT_f32) {
          replacement = CastInst::Create(CastInst::BitCast, replacement, i32Ty, "", call);

          bitOffset %= 32;

          if (bitOffset)
            replacement = BinaryOperator::CreateLShr(replacement, ConstantInt::get(i32Ty, bitOffset), "", call);

          if (bitSize != 32)
			  if (bitSize == 8)
			  {
				  replacement = BinaryOperator::Create(Instruction::And, replacement, ConstantInt::get(i32Ty, 0xFF), "", call);
			  }
			  else
			  {
				  replacement = CastInst::Create(CastInst::Trunc, replacement, scalarTypes[formatDesc.structTypeDef[column]], "", call);
			  }
        }

        if (replacement->getType() != call->getType()) {
          FormatCvtType fmtCvtType = formatDescs[formatIdx].fmtCvtType;

          switch (fmtCvtType) {
          case CVT_FLOAT:
          {
            if (bitSize == 16) {
              // NOTE: LLVM bit cast is not allowed for half when using min precision types (non-native FP16).
              //       Use dx.op.bitcastI16toF16 that accounts for min precision types.
              //replacement = CastInst::Create(CastInst::BitCast, replacement, Type::getHalfTy(context), "", call);
              replacement = CreateBitcastI16toF16(dxil, replacement, call);

              if (!call->getType()->isHalfTy()) {
                DXASSERT_NOMSG(call->getType()->isFloatTy());
                replacement = CastInst::Create(CastInst::FPExt, replacement, Type::getFloatTy(context), "", call);
              }
            }
            else if (bitSize == 32) {
              DXASSERT_NOMSG(call->getType()->isHalfTy());
              replacement = CastInst::Create(CastInst::FPTrunc, replacement, Type::getHalfTy(context), "", call);
            }
          }
          break;
          case CVT_SINT:
          {
            DXASSERT_NOMSG(call->getType()->isIntegerTy());

            if (bitSize > (int)call->getType()->getIntegerBitWidth())
              replacement = CastInst::Create(CastInst::Trunc, replacement, call->getType(), "", call);
            else
              replacement = CastInst::Create(CastInst::SExt, replacement, call->getType(), "", call);
          }
          break;
          case CVT_SNORM:
          {
            DXASSERT_NOMSG(call->getType()->isFloatTy() || call->getType()->isHalfTy());

            Value   *fpVal = CastInst::Create(CastInst::SIToFP, replacement, Type::getFloatTy(context), "", call);
            unsigned rawCompBitWidth = replacement->getType()->getIntegerBitWidth();
            Value   *rcpMaxS         = ConstantFP::get(Type::getFloatTy(context), 1.0 / ((float)((1 << (rawCompBitWidth - 1)) - 1)));

            replacement = BinaryOperator::Create(BinaryOperator::FMul, fpVal, rcpMaxS, "", call);

            if (call->getType()->isHalfTy())
              replacement = CastInst::Create(CastInst::FPTrunc, replacement, Type::getHalfTy(context), "", call);
          }
          break;
          case CVT_UINT:
          {
            DXASSERT_NOMSG(call->getType()->isIntegerTy());

            if (bitSize > (int)call->getType()->getIntegerBitWidth())
              replacement = CastInst::Create(CastInst::Trunc, replacement, call->getType(), "", call);
            else
              replacement = CastInst::Create(CastInst::ZExt, replacement, call->getType(), "", call);
          }
          break;
          case CVT_UNORM:
          {
            DXASSERT_NOMSG(call->getType()->isFloatTy() || call->getType()->isHalfTy());

            Value   *fpVal = CastInst::Create(CastInst::UIToFP, replacement, Type::getFloatTy(context), "", call);
            unsigned rawCompBitWidth = replacement->getType()->getIntegerBitWidth();
            Value   *rcpMaxU         = ConstantFP::get(Type::getFloatTy(context), 1.0 / ((float)((1 << rawCompBitWidth) - 1)));

            replacement = BinaryOperator::Create(BinaryOperator::FMul, fpVal, rcpMaxU, "", call);

            if (call->getType()->isHalfTy())
              replacement = CastInst::Create(CastInst::FPTrunc, replacement, Type::getHalfTy(context), "", call);
          }
          break;
          default:
            DXASSERT_NOMSG(0);
            break;
          }
        }
      }

      call->replaceAllUsesWith(replacement);
      call->eraseFromParent();
    }

    // Patch store output.
    for (CallInst *call : storeOutputs) {
      int signatureId = (int)cast<ConstantInt>(call->getArgOperand(1))->getSExtValue();
      int signatureIdx = SignatureElementIndexFromId(signatureId, vsEntrySig.OutputSignature);
      DXASSERT_NOMSG(signatureIdx >= 0);

      Value *rowIdx = UndefValue::get(i32Ty);

      if (ConstantInt *rowOffCnst = dyn_cast<ConstantInt>(call->getArgOperand(2))) {
        int rowIdxVal = (int)rowOffCnst->getZExtValue();

        if (rowIdxVal != 0)
          rowIdx = ConstantInt::get(i32Ty, rowIdxVal);
      }
      else {
        rowIdx = call->getArgOperand(2);
      }

      bool hasConstantIndex = isa<ConstantInt>(rowIdx);

      if (!isa<UndefValue>(rowIdx) && !hasConstantIndex) {
        // Dynamically indexed signature not supported.
        return DXC_E_NOT_SUPPORTED;
      }

      int row = 0;
      if (hasConstantIndex)
        row += (int)cast<ConstantInt>(rowIdx)->getSExtValue();

      int column = (int)cast<ConstantInt>(call->getArgOperand(3))->getZExtValue();
      int index = row*4 + column;

      DXASSERT_NOMSG(vsOutTy->getStructElementType(signatureIdx)->getVectorElementType() == call->getArgOperand(4)->getType());
      DXASSERT_NOMSG(index < (int)vsOutTy->getStructElementType(signatureIdx)->getVectorNumElements());

      Value *indices[] = { ConstantInt::get(i32Ty, 0), ConstantInt::get(i32Ty, signatureIdx), ConstantInt::get(i32Ty, index) };
      Instruction *elementPtr = GetElementPtrInst::CreateInBounds(vsOut, indices, "", call);

      new StoreInst(call->getArgOperand(4), elementPtr, call);

      call->eraseFromParent();
    }

    // Patch returns.
    for (Instruction *retInst : returns) {
      Value *loadedVsOut = new LoadInst(vsOut, "", retInst);
      retInst->replaceAllUsesWith(ReturnInst::Create(context, loadedVsOut, retInst));
      retInst->eraseFromParent();
    }
  }

  return S_OK;
}

static HRESULT ConvertPSMainInput(const DxilDeclarations &dxil, StructType *vsOutTy, DxilModule *DM, Function *psMain, Function *psOriginalFunction,
    Function *vsOriginalFunction) {
  LLVMContext &context = psMain->getContext();

  Type *i32Ty = Type::getInt32Ty(context);

  // Prepare VSOut.
  auto args = psMain->getArgumentList().begin();

  args++;
  Value *vsOut = args;

  SmallVector<Value*, 8> vsOutMembers;
  vsOutMembers.resize(vsOutTy->getNumElements());

  Instruction *insertBefore = psMain->begin()->begin();

  for (size_t i = 0; i < vsOutMembers.size(); ++i)
    vsOutMembers[i] = ExtractValueInst::Create(vsOut, (unsigned)i, "", insertBefore);

  // Find instructions to patch.
  std::vector<CallInst*> loadInputs;

  for (Function &func : *psMain->getParent()) {
    if (func.getName().startswith("dx.op.loadInput."))
      AppendCallsToFunction(&func, psMain, loadInputs);
  }

  DxilEntrySignature psEntrySig = DM->GetDxilEntrySignature(psOriginalFunction);
  DxilEntrySignature vsEntrySig = DM->GetDxilEntrySignature(vsOriginalFunction);

  // Patch loadInput.
  for (CallInst *call : loadInputs) {
    int signatureId = (int)cast<ConstantInt>(call->getArgOperand(1))->getSExtValue();
    int psSignatureIdx = SignatureElementIndexFromId(signatureId, psEntrySig.InputSignature);
    DXASSERT_NOMSG(psSignatureIdx >= 0);

    const DxilSignatureElement &signature = psEntrySig.InputSignature.GetElement(psSignatureIdx);

    if (signature.GetSemantic()->GetKind() == DXIL::SemanticKind::IsFrontFace) {
      Value *hitKind = CreateGetHitKind(dxil, call);

      Type *i1Ty = Type::getInt1Ty(context);
      Value *isFrontFacei1 = CastInst::Create(Instruction::Trunc, hitKind, i1Ty, "", call);
      isFrontFacei1 = BinaryOperator::Create(Instruction::Xor, isFrontFacei1, ConstantInt::get(i1Ty, 1), "", call);

      Value *isFrontFace = CastInst::Create(Instruction::ZExt, isFrontFacei1, call->getType(), "", call);
      call->replaceAllUsesWith(isFrontFace);
      call->eraseFromParent();
      continue;
    }

    Value *rowIdx = UndefValue::get(i32Ty);

    if (ConstantInt *rowOffCnst = dyn_cast<ConstantInt>(call->getArgOperand(2))) {
      int rowIdxVal = (int)rowOffCnst->getZExtValue();

      if (rowIdxVal != 0)
        rowIdx = ConstantInt::get(i32Ty, rowIdxVal);
    }
    else {
      rowIdx = call->getArgOperand(2);
    }

    bool hasConstantIndex = isa<ConstantInt>(rowIdx);

    if (!isa<UndefValue>(rowIdx) && !hasConstantIndex) {
      // Dynamically indexed signature not supported.
      return DXC_E_NOT_SUPPORTED;
    }

    int row = 0;
    if (hasConstantIndex)
      row += (int)cast<ConstantInt>(rowIdx)->getSExtValue();

    // Find the corresponding VS signature and use that.
    int vsSignatureIdx = 0;
    {
      StringRef semantic(signature.GetSemanticName());
      unsigned semanticIndex = signature.GetSemanticIndexVec()[row];

      const std::vector<std::unique_ptr<DxilSignatureElement> > &elements =
        vsEntrySig.OutputSignature.GetElements();

      for (; vsSignatureIdx < (int)elements.size(); ++vsSignatureIdx) {
        if (semantic.compare_lower(elements[vsSignatureIdx]->GetSemanticName()) == 0) {
          const std::vector<unsigned> &indices = elements[vsSignatureIdx]->GetSemanticIndexVec();
          
          size_t i = 0;

          for (; i < indices.size(); ++i) {
            if (semanticIndex == indices[i])
              break;
          }

          if (i < indices.size()) {
            row = (int)i;
            break;
          }
        }
      }

      DXASSERT_NOMSG(vsSignatureIdx < elements.size());
    }

    int column = (int)cast<ConstantInt>(call->getArgOperand(3))->getZExtValue();
    int index = row*4 + column;

    DXASSERT_NOMSG(vsOutMembers[vsSignatureIdx]->getType()->getVectorElementType() == call->getType());
    DXASSERT_NOMSG(index < (int)vsOutMembers[vsSignatureIdx]->getType()->getVectorNumElements());
    Value *replacement = ExtractElementInst::Create(vsOutMembers[vsSignatureIdx], ConstantInt::get(i32Ty, index), "", call);
    call->replaceAllUsesWith(replacement);
    call->eraseFromParent();
  }

  return S_OK;
}

static HRESULT PatchStoreOutput(Function *hitMain, const DxilEntrySignature& psEntrySig,
  int unmaskedPayloadSize, int payloadSize, UINT64 psRendertargetScalarOutputMask) {

  LLVMContext &context = hitMain->getContext();

  Type *i32Ty = Type::getInt32Ty(context);
  Type *f32Ty = Type::getFloatTy(context);

  Value *payloadPtr = hitMain->getArgumentList().begin();

  // Find instructions to patch.
  std::vector<CallInst*> storeOutputs;
  for (Function &func : *hitMain->getParent()) {
    if (func.getName().startswith("dx.op.storeOutput"))
      AppendCallsToFunction(&func, hitMain, storeOutputs);
  }

  // Patch storeOutput.
  for (CallInst *call : storeOutputs) {
    int signatureId = (int)cast<ConstantInt>(call->getArgOperand(1))->getSExtValue();
    int signatureIdx = SignatureElementIndexFromId(signatureId, psEntrySig.OutputSignature);
    DXASSERT_NOMSG(signatureIdx >= 0);

    const DxilSignatureElement &signature = psEntrySig.OutputSignature.GetElement(signatureIdx);

    Value *rowIdx = UndefValue::get(i32Ty);

    if (ConstantInt *rowOffCnst = dyn_cast<ConstantInt>(call->getArgOperand(2))) {
      int rowIdxVal = (int)rowOffCnst->getZExtValue();

      if (rowIdxVal != 0)
        rowIdx = ConstantInt::get(i32Ty, rowIdxVal);
    }
    else {
      rowIdx = call->getArgOperand(2);
    }

    bool hasConstantIndex = isa<ConstantInt>(rowIdx);
    DXASSERT(isa<UndefValue>(rowIdx) || hasConstantIndex, "Dynamically indexed shader output not yet supported");

    int row = signature.GetStartRow();
    if (hasConstantIndex)
      row += (int)cast<ConstantInt>(rowIdx)->getSExtValue();

    int column = signature.GetStartCol() + (int)cast<ConstantInt>(call->getArgOperand(3))->getZExtValue();
    int index = row*4 + column;
    DXASSERT_NOMSG(index < unmaskedPayloadSize);

    if (psRendertargetScalarOutputMask & (1llu << index)) {
      int maskedIndex = __popcnt64(psRendertargetScalarOutputMask & ((1llu << index) - 1));
      DXASSERT_NOMSG(maskedIndex < payloadSize);

      Value *indices[] = { ConstantInt::get(i32Ty, 0), ConstantInt::get(i32Ty, maskedIndex) };
      Value *elementPtr = GetElementPtrInst::CreateInBounds(payloadPtr, indices, "", call);
      Value *value = call->getArgOperand(4);

      if (!value->getType()->isFloatTy())
        value = CastInst::Create(Instruction::BitCast, value, f32Ty, "", call);

      new StoreInst(value, elementPtr, call);
    }

    call->eraseFromParent();
  }

  return S_OK;
}

static void AdjustSpaces( DxilModule * pDM, UINT spaceOffset )
{
    for( auto &resource : pDM->GetCBuffers() )
    {
        resource->SetSpaceID( resource->GetSpaceID() + spaceOffset );
    }
    for( auto &resource : pDM->GetSamplers() )
    {
        resource->SetSpaceID( resource->GetSpaceID() + spaceOffset );
    }
    for( auto &resource : pDM->GetSRVs() )
    {
        resource->SetSpaceID( resource->GetSpaceID() + spaceOffset );
    }
    for( auto &resource : pDM->GetUAVs() )
    {
        resource->SetSpaceID( resource->GetSpaceID() + spaceOffset );
    }
}

static void RenameLibContents( Module * pModule, StringRef nameAppend )
{
    DxilModule *pDM = &pModule->GetDxilModule();

    for( auto &func : pModule->getFunctionList() )
    {
        if( func.getName().startswith( "dx.op" ) )
            continue;
        func.setName( Twine( func.getName() ).concat( Twine( nameAppend ) ) );
    }
    for( auto &resource : pDM->GetCBuffers() )
    {
        resource->SetGlobalName( resource->GetGlobalName().c_str() + nameAppend.str() );
        if (GlobalVariable *GV = dyn_cast<GlobalVariable>(resource->GetGlobalSymbol())) {
          GV->setName( Twine( GV->getName() ).concat( Twine( nameAppend ) ) );
        }
    }
    for( auto &resource : pDM->GetSamplers() )
    {
        resource->SetGlobalName( resource->GetGlobalName().c_str() + nameAppend.str() );
        if (GlobalVariable *GV = dyn_cast<GlobalVariable>(resource->GetGlobalSymbol())) {
          GV->setName( Twine( GV->getName() ).concat( Twine( nameAppend ) ) );
        }
    }
    for( auto &resource : pDM->GetSRVs() )
    {
        resource->SetGlobalName( resource->GetGlobalName().c_str() + nameAppend.str() );
        if (GlobalVariable *GV = dyn_cast<GlobalVariable>(resource->GetGlobalSymbol())) {
          GV->setName( Twine( GV->getName() ).concat( Twine( nameAppend ) ) );
        }
    }
    for( auto &resource : pDM->GetUAVs() )
    {
        resource->SetGlobalName( resource->GetGlobalName().c_str() + nameAppend.str() );
        if (GlobalVariable *GV = dyn_cast<GlobalVariable>(resource->GetGlobalSymbol())) {
          GV->setName( Twine( GV->getName() ).concat( Twine( nameAppend ) ) );
        }
    }
}

static HRESULT VsPsToHitShaders(
  Module                   *pModule,
  DxilModule               *DM,
  DxilModule               *vsDxil,
  DxilModule               *psDxil,
  const StringRef           pVsLibEntrypoint,
  const StringRef           pPsLibEntrypoint,
  const DxcInputLayoutDesc &dxcInputLayout,
  const UINT               *vertexBufferStrides,
  UINT                      indexByteSize,
  UINT64                    psRendertargetScalarOutputMask,
  bool                      forAnyHitShader,
  UINT                      iaRegisterSpace,
  UINT                      vsRegisterSpaceAddend,
  UINT                      psRegisterSpaceAddend,
  std::unique_ptr<Module>  *outHitShaderModule) {

  LLVMContext &context = pModule->getContext();

  Type *i32Ty = Type::getInt32Ty(context);
  Type *f32Ty = Type::getFloatTy(context);

  if ( !DM || DM->GetShaderModel()->GetKind() != DXIL::ShaderKind::Library )
    return E_INVALIDARG;

  // Find the entrypoint functions necessary to assemble the AHS/CHS.
  Function *vsFunction = pModule->getFunction( std::string( pVsLibEntrypoint ) + "_VS" );
  Function *psFunction = pModule->getFunction( std::string( pPsLibEntrypoint ) + "_PS" );

  if (!vsFunction || !psFunction)
    return E_INVALIDARG;

  DxilDeclarations dxil = GenerateDxilDeclarations(pModule);

  // Determine payload size.
  if( !DM->HasDxilEntrySignature( vsFunction ) || !DM->HasDxilEntrySignature( psFunction ) )
  {
      DXASSERT(false, "VS/PS functions are missing dxil entry signatures.");
  }
  DxilEntrySignature vsEntrySig = DM->GetDxilEntrySignature( vsFunction );
  DxilEntrySignature psEntrySig = DM->GetDxilEntrySignature( psFunction );

  int unmaskedPayloadSize = 0;
  {
    const std::vector<std::unique_ptr<DxilSignatureElement>> &elements =
      psEntrySig.OutputSignature.GetElements();

    for (size_t i = 0; i < elements.size(); ++i) {
      const DxilSignatureElement &s = *elements[i];
      int end = (s.GetStartRow() + (s.GetRows()-1))*4 + s.GetStartCol() + s.GetCols();

      if (unmaskedPayloadSize < end)
        unmaskedPayloadSize = end;
    }
  }

  psRendertargetScalarOutputMask &= (1llu << unmaskedPayloadSize)-1;

  int payloadSize = __popcnt64(psRendertargetScalarOutputMask);

  // Create payload and attribute types.
  Type *payloadTy = StructType::create(context, std::vector<Type*>(payloadSize, f32Ty), "struct.Payload");
  Type *attributeTy = StructType::create(context, std::vector<Type*>(2, f32Ty), "struct.Attributes");

  // Determine input elements.
  SmallVector<VsInputElementDesc, 8> inputLayout;
  DxcInputLayoutToVsInputElementLayout(dxcInputLayout, inputLayout,
                                       vsEntrySig.InputSignature.GetElements());

  // Setup VSOut support.
  StructType *vsOutTy =
      CreateVSOutType(context, vsEntrySig.OutputSignature.GetElements());
  Function *vsOutAdd = GenerateVSOutAdd( pModule, vsOutTy, vsEntrySig.OutputSignature.GetElements());
  Function *vsOutMul = GenerateVSOutMul( pModule, vsOutTy, vsEntrySig.OutputSignature.GetElements());

  // Import VS and PS.
  ValueToValueMapTy valueMap;

  Type *vsMainArgs[] = {i32Ty, i32Ty};
  Function *vsMain = cast<Function>(pModule->getOrInsertFunction(
      "NewVSMain", FunctionType::get(vsOutTy, vsMainArgs, false)));
  vsMain->addFnAttr(Attribute::AlwaysInline);
  ImportFunctionInto(valueMap, dxil, vsFunction, vsMain);

  Type *psMainArgs[] = {payloadTy->getPointerTo(), vsOutTy};
  Function *psMain = cast<Function>(pModule->getOrInsertFunction(
      "NewPSMain",
      FunctionType::get(Type::getVoidTy(context), psMainArgs, false)));
  psMain->addFnAttr(Attribute::AlwaysInline);
  ImportFunctionInto(valueMap, dxil, psFunction, psMain);

  // Merge resources.
  std::vector<std::unique_ptr<DxilResource>> srvResources;
  std::vector<std::unique_ptr<DxilResource>> uavResources;
  std::vector<std::unique_ptr<DxilCBuffer>> cbvResources;
  std::vector<std::unique_ptr<DxilSampler>> samplerResources;

  MergeResources(srvResources, uavResources, cbvResources, samplerResources, DM, vsDxil, psDxil, vsMain, psMain, vsRegisterSpaceAddend, psRegisterSpaceAddend);

  // Generate HitMain.
  std::string hitMainName = std::string("__") + (forAnyHitShader ? "AHS" : "CHS") + std::string("_from_") + pVsLibEntrypoint.str() + "_" + pPsLibEntrypoint.str();

  // TODO: Should hitMain be mangled or not?
  hitMainName = "\01?" + hitMainName + "@@YAXUPayload@@UAttributes@@@Z";

  Function *hitMain = GenerateHitMain(hitMainName, indexByteSize, dxil, payloadTy, attributeTy, vsMain, psMain, vsOutMul, vsOutAdd, iaRegisterSpace, srvResources, cbvResources);

  if (!hitMain)
    return E_FAIL;

  // Convert input/output.
  HRESULT result = ConvertVSMainInputOutput(dxil, vsMain, vsFunction, DM, inputLayout, indexByteSize, vertexBufferStrides, iaRegisterSpace, srvResources);

  if (FAILED(result))
    return result;

  result = ConvertPSMainInput(dxil, vsOutTy, DM, psMain, psFunction, vsFunction);

  if (FAILED(result))
    return result;

  // Init DXIL module. This is needed for inlining to work.
  pModule->ResetDxilModule();
  DxilModule &dxilModule = pModule->GetOrCreateDxilModule(true);

  dxilModule.SetShaderModel(ShaderModel::GetByName("lib_6_3"));
  dxilModule.SetValidatorVersion(1, 3);

  // Remove old functions
  vsFunction->eraseFromParent();
  psFunction->eraseFromParent();
  vsMain->setName( pVsLibEntrypoint );
  psMain->setName( pPsLibEntrypoint );

  // Inline functions.
  {
    legacy::PassManager PM;
    PM.add(createAlwaysInlinerPass());
    PM.run(*pModule);

    vsMain->eraseFromParent();
    psMain->eraseFromParent();
    vsOutAdd->eraseFromParent();
    vsOutMul->eraseFromParent();
  }

  // Cleanup for validation - scalarize VSOut struct.
  {
    legacy::PassManager PM;

    // Make sure VSOut is in SSA form.
    PM.add(createSROAPass());
    PM.add(createPromoteMemoryToRegisterPass());

    // Eliminate insertvalue/extractvalue (should be all that remains from VSOut).
    PM.add(createInstructionCombiningPass());

    // Eliminate remaining vector operations.
    PM.add(createScalarizerPass());
    PM.run(*pModule);
  }

  PatchStoreOutput(hitMain, psEntrySig, unmaskedPayloadSize, payloadSize, psRendertargetScalarOutputMask);

  // Patch trace and discard.
  SmallVector<Function*, 8> functionsToRemove;

  for (Function &func : *pModule) {
    if (func.getName().startswith("dx.op.traceRay.") && forAnyHitShader) {
      std::vector<CallInst*> calls;
      AppendCallsToFunction(&func, nullptr, calls);

      for (CallInst *inst : calls)
        inst->eraseFromParent();

      functionsToRemove.push_back(&func);
    } else if (func.getName().startswith("dx.op.discard")) {
      std::vector<CallInst*> calls;
      AppendCallsToFunction(&func, nullptr, calls);

      for (CallInst *inst : calls) {
        BasicBlock *parentBB = inst->getParent();
        Function *function = parentBB->getParent();
        Value *discard = inst->getArgOperand(1);
        Instruction *next = ++BasicBlock::iterator(inst);
        inst->eraseFromParent();

        if (forAnyHitShader) {
          if (isa<ReturnInst>(next))
            continue;

          BasicBlock *returnBB = BasicBlock::Create(context, "discard_return", function);
          CreateIgnoreIntersection(dxil, returnBB);
          ReturnInst::Create(context, returnBB);

          BasicBlock *secondBB = parentBB->splitBasicBlock(next);
          BranchInst *firstBBterminator = cast<BranchInst>(parentBB->getTerminator());
          BranchInst::Create(returnBB, secondBB, discard, firstBBterminator);
          firstBBterminator->eraseFromParent();
        }
      }
      functionsToRemove.push_back(&func);
    }
  }

  for (Function *func : functionsToRemove)
    func->eraseFromParent();

  // Remove dead code.
  {
    legacy::PassManager PM;
    PM.add(createAggressiveDCEPass());

    PM.run(*pModule);
  }

  // Emit resources.
  for (std::unique_ptr<DxilResource> &resource : srvResources)
    dxilModule.AddSRV(std::move(resource));

  for (std::unique_ptr<DxilResource> &resource : uavResources)
    dxilModule.AddUAV(std::move(resource));

  for (std::unique_ptr<DxilCBuffer> &resource : cbvResources)
      dxilModule.AddCBuffer(std::move(resource));

  for (std::unique_ptr<DxilSampler> &resource : samplerResources)
    dxilModule.AddSampler(std::move(resource));

  DxilFunctionAnnotation *hitMainAnnotation = dxilModule.GetTypeSystem().AddFunctionAnnotation(hitMain);

  DxilParameterAnnotation &payloadAnnotation = hitMainAnnotation->GetParameterAnnotation(0);
  payloadAnnotation.SetParamInputQual(DxilParamInputQual::Inout);
  payloadAnnotation.SetSemanticString("SV_RayPayload");

  DxilParameterAnnotation &attributeAnnotation = hitMainAnnotation->GetParameterAnnotation(1);
  attributeAnnotation.SetParamInputQual(DxilParamInputQual::In);
  attributeAnnotation.SetSemanticString("SV_IntersectionAttributes");

  std::unique_ptr<DxilFunctionProps> hitMainProps(new DxilFunctionProps());

  hitMainProps->shaderKind = forAnyHitShader ? DXIL::ShaderKind::AnyHit : DXIL::ShaderKind::ClosestHit;

  hitMainProps->ShaderProps.Ray.payloadSizeInBytes = payloadSize * sizeof(float);
  hitMainProps->ShaderProps.Ray.attributeSizeInBytes = 2 * sizeof(float);

  // Set entry props
  bool bUseMinPrecision = vsEntrySig.InputSignature.UseMinPrecision();
  std::unique_ptr<DxilEntryProps> pProps = llvm::make_unique<DxilEntryProps>(*hitMainProps.get(), bUseMinPrecision);

  DxilEntryPropsMap entryProps; 
  entryProps[hitMain] = std::move(pProps);

  dxilModule.ResetEntryPropsMap(std::move(entryProps));
  dxilModule.ClearDxilMetadata( *pModule );


  // Emit Dxil Metadata
  {
    legacy::PassManager PM;
    PM.add(createDxilEmitMetadataPass());

    PM.run(*pModule);
  }

  // Cleanup for validation - convert dx.op.sample.
  {
    for (Function& function : *pModule) {
      if (!function.getName().startswith("dx.op.sample."))
        continue;

      SmallVector<Type*, 16> paramTypes;

      for (size_t i = 0; i < function.getFunctionType()->getNumParams(); ++i)
        paramTypes.push_back(function.getFunctionType()->getParamType(i));

      FunctionType* functionType = FunctionType::get(function.getFunctionType()->getReturnType(), paramTypes, false);
      Twine functionName = Twine("dx.op.sampleLevel.") + function.getName().substr(13);

      Function* replacement = cast<Function>(pModule->getOrInsertFunction(functionName.str(), functionType, function.getAttributes()));

      std::vector<CallInst*> calls;

      for (User* user : function.users()) {
        if (CallInst* call = dyn_cast<CallInst>(user))
          calls.push_back(call);
      }

      for (CallInst* call : calls) {
        call->setOperand(0, ConstantInt::get(Type::getInt32Ty(context), (int)OP::OpCode::SampleLevel));
        call->setOperand(paramTypes.size() - 1, ConstantFP::get(Type::getFloatTy(context), 0.0));
        call->setCalledFunction(replacement);
      }
    }
  }

  // Cleanup for validation - convert dx.op.sampleBias.
  {
    for (Function& function : *pModule) {
      if (!function.getName().startswith("dx.op.sampleBias."))
        continue;

      SmallVector<Type*, 16> paramTypes;

      for (size_t i = 0; i < function.getFunctionType()->getNumParams(); ++i)
        paramTypes.push_back(function.getFunctionType()->getParamType(i));

      paramTypes.pop_back();

      FunctionType* functionType = FunctionType::get(function.getFunctionType()->getReturnType(), paramTypes, false);
      Twine functionName = Twine("dx.op.sampleLevel.") + function.getName().substr(17);

      Function* replacement = cast<Function>(pModule->getOrInsertFunction(functionName.str(), functionType, function.getAttributes()));

      std::vector<CallInst*> calls;

      for (User* user : function.users()) {
        if (CallInst* call = dyn_cast<CallInst>(user))
          calls.push_back(call);
      }

      for (CallInst* call : calls) {
        SmallVector<Value*, 16> args;

        for (unsigned i = 0; i < call->getNumArgOperands(); ++i)
          args.push_back(call->getArgOperand(i));

        args.pop_back();
        args[0] = ConstantInt::get(Type::getInt32Ty(context), (int)OP::OpCode::SampleLevel);
        args[args.size()-1] = ConstantFP::get(Type::getFloatTy(context), 0.0);

        CallInst* newCall = CallInst::Create(replacement, args, "", call);
        call->replaceAllUsesWith(newCall);
        call->eraseFromParent();
      }
    }
  }

  // Cleanup for validation - remove unused functions and resources.
  {
    DxilModule &dxilModule = pModule->GetOrCreateDxilModule();

    std::vector<Function*> functionsToRemove;

    for (Function& function : *pModule) {
      if (function.isDeclaration())
        continue;

      if (dxilModule.HasDxilEntryProps(&function))
        continue;

      functionsToRemove.push_back(&function);
    }

    for (Function* function : functionsToRemove) {
      dxilModule.RemoveFunction(function);
      function->eraseFromParent();
    }

    functionsToRemove.resize(0);

    for (Function& function : *pModule) {
      if (function.isDeclaration() && function.uses().begin() == function.uses().end())
        functionsToRemove.push_back(&function);
    }

    for (Function* function : functionsToRemove) {
      dxilModule.RemoveFunction(function);
      function->eraseFromParent();
    }

    dxilModule.RemoveUnusedResourceSymbols();
    dxilModule.ReEmitDxilResources();
  }

  pModule->ResetDxilModule();

  DXASSERT_NOMSG(!verifyModule(*pModule, &dbgs()));

  return S_OK;
}

static Module *CreateModuleFromBlob(LLVMContext &context, IDxcBlob *pBlob) {
  const DxilContainerHeader *pContainer = IsDxilContainerLike(
    pBlob->GetBufferPointer(), pBlob->GetBufferSize());

  if (!IsValidDxilContainer(pContainer, pBlob->GetBufferSize()))
    return nullptr;

  const DxilProgramHeader *pProgramHeader = GetDxilProgramHeader(pContainer, DFCC_DXIL);

  if (!pProgramHeader)
    return nullptr;

  const char *pBitcode;
  uint32_t bitcodeLength;
  GetDxilProgramBitcode(pProgramHeader, &pBitcode, &bitcodeLength);

  std::unique_ptr<MemoryBuffer> pBitcodeBuf(MemoryBuffer::getMemBuffer(StringRef(pBitcode, bitcodeLength), "", false));
  ErrorOr<std::unique_ptr<Module>> pModule(parseBitcodeFile(pBitcodeBuf->getMemBufferRef(), context));

  if (pModule.getError())
    return nullptr;

  return pModule.get().release();
}

static HRESULT CreateBlobFromModule(IMalloc *pMalloc, Module *pModule, IDxcBlob **ppBlob) {
    CComPtr<AbstractMemoryStream> pModuleBitcode;
    HRESULT result = CreateMemoryStream(pMalloc, &pModuleBitcode);

    if (FAILED(result))
        return result;

    raw_stream_ostream outStream(pModuleBitcode.p);
    WriteBitcodeToFile(pModule, outStream);
    outStream.flush();

    CComPtr<AbstractMemoryStream> pContainerStream;
    result = CreateMemoryStream(pMalloc, &pContainerStream);

    if (FAILED(result))
        return result;

    SerializeDxilContainerForModule(&pModule->GetOrCreateDxilModule(),
        pModuleBitcode, pContainerStream,
        SerializeDxilFlags::None);

    CComPtr<IDxcOperationResult> pValResult;

    IDxcBlob *pOutputBlob = nullptr;
    pContainerStream->QueryInterface(&pOutputBlob);

    CComPtr<IDxcValidator> pValidator;

#if VSPS_DXIL_EXTERNAL_VALIDATOR
    DxilLibCreateInstance(CLSID_DxcValidator, &pValidator);
    if( pValidator == nullptr )
        return E_NOINTERFACE;
    pValidator->Validate(pOutputBlob, DxcValidatorFlags_InPlaceEdit, &pValResult);
#else
    CreateDxcValidator(IID_PPV_ARGS(&pValidator));
    RunInternalValidator(pValidator, pModule, nullptr, pOutputBlob,
        DxcValidatorFlags_InPlaceEdit, &pValResult);
#endif

    HRESULT valHR;
    pValResult->GetStatus(&valHR);

    if (FAILED(valHR)) {
        CComPtr<IDxcBlobEncoding> pErrors;
        CComPtr<IDxcBlobEncoding> pErrorsUtf8;
        IFT(pValResult->GetErrorBuffer(&pErrors));
        IFT(hlsl::DxcGetBlobAsUtf8(pErrors, &pErrorsUtf8));
        StringRef errRef((const char *)pErrorsUtf8->GetBufferPointer(),
            pErrorsUtf8->GetBufferSize());
        OutputDebugStringA(errRef.str().c_str());
        exit(1);
    }

    CComPtr<IDxcBlob> pValidatedBlob;
    IFT(pValResult->GetResult(&pValidatedBlob));
    if (pValidatedBlob != nullptr) {
        pOutputBlob->Release();
        pOutputBlob = pValidatedBlob.Detach();
    }

    pValidator.Release();

    *ppBlob = pOutputBlob;
    return S_OK;
}

static bool IsLib(Module *pModule) {
  NamedMDNode* md = pModule->getNamedMetadata("dx.shaderModel");

  if (!md || md->getNumOperands() < 1)
    return false;

  MDNode* shaderModel = md->getOperand(0);

  if (!shaderModel || shaderModel->getNumOperands() < 1)
    return false;

  Metadata* name = shaderModel->getOperand(0);

  return name && isa<MDString>(name) && cast<MDString>(name)->getString() == "lib";
}

static void unmangleFunctionName(StringRef& name) {
  StringRef prefix("\01?");

  if (!name.startswith(prefix))
    return;

  const char* data = name.data();
  size_t length = name.size();

  data += prefix.size();
  length -= prefix.size();

  size_t i = 0;
  while (i < length && data[i] != '@')
    ++i;

  name = StringRef(data, i);
}

class DxcVsPsToHitShader : public IDxcVsPsToHitShader {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcVsPsToHitShader)
  
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcVsPsToHitShader>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE Transform(
    _In_ IDxcBlob              *pVsShader,
    _In_opt_ LPCWSTR            pVsLibEntrypoint,
    _In_ IDxcBlob              *pPsShader,
    _In_opt_ LPCWSTR            pPsLibEntrypoint,
    DxcInputLayoutDesc          inputLayout,
    UINT                        indexByteSize,
    const UINT                 *pVertexBufferStrides,
    UINT                        iaRegisterSpace,
    UINT                        vsRegisterSpaceAddend,
    UINT                        psRegisterSpaceAddend,
    UINT64                      chRendertargetScalarOutputMask,
    UINT64                      ahRendertargetScalarOutputMask,
    _COM_Outptr_opt_ IDxcBlob **ppChShader,
    _COM_Outptr_opt_ IDxcBlob **ppAhShader
  ) override {
    if (!pVsShader || !pPsShader)
      return E_INVALIDARG;

    if (indexByteSize != 4 && indexByteSize != 2 && indexByteSize != 0)
      return E_INVALIDARG;

    if (!ppChShader && !ppAhShader)
      return E_INVALIDARG;

    CW2A vsEntrypointName(pVsLibEntrypoint, CP_UTF8);
    CW2A psEntrypointName(pPsLibEntrypoint, CP_UTF8);

    DxcThreadMalloc TM(m_pMalloc);

    LLVMContext context;
    std::unique_ptr<Module> pVsModule(CreateModuleFromBlob(context, pVsShader));
    std::unique_ptr<Module> pPsModule(CreateModuleFromBlob(context, pPsShader));

    if (!pVsModule || !pPsModule)
      return DXC_E_INCORRECT_DXBC;

    DxilModule *vsDxil = DxilModule::TryGetDxilModule( pVsModule.get() );
    DxilModule *psDxil = DxilModule::TryGetDxilModule( pPsModule.get() );

    AdjustSpaces( vsDxil, vsRegisterSpaceAddend );
    AdjustSpaces( psDxil, psRegisterSpaceAddend );
    RenameLibContents( pVsModule.get(), "_VS" );
    RenameLibContents( pPsModule.get(), "_PS" );

    // Emit updated Dxil Metadata
    {
        legacy::PassManager PM;
        PM.add(createDxilEmitMetadataPass());
        PM.run(*pVsModule.get());
    }
    {
        legacy::PassManager PM;
        PM.add(createDxilEmitMetadataPass());
        PM.run(*pPsModule.get());
    }

    // Link incoming libraries into a new library.
    std::unique_ptr<Module> pCHSModule;
    dxilutil::ExportMap exportMap;
    std::unique_ptr<DxilLinker> pLinker(DxilLinker::CreateLinker(context, 1, 3));
    pLinker->RegisterLib("VS", std::move(pVsModule), nullptr);
    pLinker->AttachLib("VS");
    pLinker->RegisterLib( "PS", std::move( pPsModule ), nullptr );
    pLinker->AttachLib( "PS" );
    pCHSModule = std::move(pLinker->Link("", "lib_6_x", exportMap));
    std::unique_ptr<Module> pAHSModule( CloneModule( pCHSModule.get() ) );
    DxilModule *pCHSDM = DxilModule::TryGetDxilModule( pCHSModule.get() );
    DxilModule *pAHSDM = DxilModule::TryGetDxilModule( pAHSModule.get() );

    if (!pCHSModule || !pAHSModule)
        return E_FAIL;

    CComPtr<IDxcBlob> pChBlob;
    CComPtr<IDxcBlob> pAhBlob;

    if (ppChShader) {
      std::unique_ptr<Module> pModule;

      HRESULT result = VsPsToHitShaders(pCHSModule.get(), pCHSDM, vsDxil, psDxil,
          StringRef(vsEntrypointName.m_psz), StringRef(psEntrypointName.m_psz), inputLayout, pVertexBufferStrides, indexByteSize,
          chRendertargetScalarOutputMask, false,
        iaRegisterSpace, vsRegisterSpaceAddend, psRegisterSpaceAddend, &pModule);

      if (FAILED(result))
        return result;

      result = CreateBlobFromModule(m_pMalloc, pCHSModule.get(), &pChBlob);

      if (FAILED(result))
        return result;
    }

    if (ppAhShader) {
        std::unique_ptr<Module> pModule;
        
        HRESULT result = VsPsToHitShaders(pAHSModule.get(), pAHSDM, vsDxil, psDxil,
          StringRef(vsEntrypointName.m_psz), StringRef(psEntrypointName.m_psz), inputLayout, pVertexBufferStrides, indexByteSize,
          ahRendertargetScalarOutputMask, true,
        iaRegisterSpace, vsRegisterSpaceAddend, psRegisterSpaceAddend, &pModule);

      if (FAILED(result))
        return result;

      result = CreateBlobFromModule(m_pMalloc, pAHSModule.get(), &pAhBlob);

      if (FAILED(result))
        return result;
    }

    if (ppChShader)
      *ppChShader = pChBlob.Detach();

    if (ppAhShader)
      *ppAhShader = pAhBlob.Detach();

    return S_OK;
  }
};

HRESULT CreateDxcVsPsToHitShader(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  CComPtr<DxcVsPsToHitShader> result = DxcVsPsToHitShader::Alloc(DxcGetThreadMallocNoRef());
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}
