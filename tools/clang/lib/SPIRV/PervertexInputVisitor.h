//===--- PervertexInputVisitor.h ---- PerVertex Input Visitor -------------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_PERVERTEXINPUTVISITOR_H
#define LLVM_CLANG_LIB_SPIRV_PERVERTEXINPUTVISITOR_H

#include "clang/AST/ASTContext.h"
#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvVisitor.h"
#include "clang/SPIRV/SpirvModule.h"
#include "clang/SPIRV/SpirvBuilder.h"

namespace clang {
namespace spirv {

class PervertexInputVisitor : public Visitor {
public:
  PervertexInputVisitor(SpirvBuilder &spvBuilder, ASTContext &astCtx, SpirvContext &spvCtx,
                        const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx), inEntryFunctionWrapper(false),
        spirvBuilder(spvBuilder), astContext(astCtx),
        currentMod(nullptr), currentFunc(nullptr) {}

  ///< Don't add extra index to a simple vector/matrix elem access when base is
  ///< not expanded.
  bool isNotExpandedVectorAccess(QualType baseType, QualType resultType);

  ///< After expanding inputs, location need reassignment.
  void reassignInputsLocation();

  ///< Expand nointerpolation decorated variables/parameters.
  ///< If a variable/parameter is passed from a decorated inputs, it should be treated as
  ///< nointerpolated too.
  bool expandNointerpVarAndParam(SpirvInstruction *spvInst);

  bool expandNointerpStructure(QualType qtype, bool isVarDecoratedInterp);

  ///< Add temp function variables, for operand replacement. An original usage to
  ///< a nointerpolated variable/parameter should be treated as an access to
  ///< its first element after expanding (data at first provoking vertex).
  SpirvInstruction *
  createFirstPerVertexVar(SpirvInstruction *base, llvm::StringRef varName);

  SpirvVariable* addFunctionTempVar(llvm::StringRef name, QualType valueType,
                                    SourceLocation loc, bool isPrecise);

  SpirvInstruction* createProvokingVertexAccessChain(SpirvInstruction* base,
                                                     uint32_t index, QualType resultType);

  ///< Get mapped operand used to replace original operand, if not exists, return itself.
  SpirvInstruction *getMappedReplaceInstr(SpirvInstruction *i);

  ///< For bool type input, add extra NE and related ops to satisfy workaround in HLSL-SPIRV.
  SpirvInstruction* getNEInstrForStgLoading(SpirvInstruction *instr);

  ///< For expanded variables, we need to decide where to add an extra index zero for
  ///< SpirvAccessChain and SpirvCompositeExtract.
  ///< This comes to two access cases : 1. array element. 2. vector channel. 
  int appendIndexZeroAt(QualType base, llvm::ArrayRef<uint32_t> index);

  ///< When use temp variables within a function, we need to add load/store ops.
  ///< TIP: A nointerpolated input or function parameter will be treated as input.vtx0
  ///<      within current function, but would be treated as an array will pass to a
  ///<      function call.
  SpirvInstruction *createVertexLoad(SpirvInstruction *base);

  void createVertexStore(SpirvInstruction *pt, SpirvInstruction *obj);

  ///< Visit different SPIR-V constructs for emitting.
  using Visitor::visit;
  bool visit(SpirvModule *, Phase phase) override;
  bool visit(SpirvFunction *, Phase phase) override;
  bool visit(SpirvEntryPoint *) override;
  bool visit(SpirvDecoration *) override;
  bool visit(SpirvVariable *) override;
  bool visit(SpirvFunctionParameter *) override;
  bool visit(SpirvAccessChain *) override;
  bool visit(SpirvCompositeExtract *) override;
  bool visit(SpirvFunctionCall *) override;

#define REMAP_FUNC_OP(CLASS)                                                   \
  bool visit(Spirv##CLASS *op) override {                                      \
    op->operandReplace([this](SpirvInstruction *inst) {                        \
      return getMappedReplaceInstr(inst);                                      \
    }, inEntryFunctionWrapper);                                                \
    return true;                                                               \
  }

  REMAP_FUNC_OP(ImageQuery)
  REMAP_FUNC_OP(ImageOp)
  REMAP_FUNC_OP(ExtInst)
  REMAP_FUNC_OP(Atomic)
  REMAP_FUNC_OP(NonUniformBinaryOp)
  REMAP_FUNC_OP(BitFieldInsert)
  REMAP_FUNC_OP(BitFieldExtract)
  REMAP_FUNC_OP(IntrinsicInstruction)
  REMAP_FUNC_OP(VectorShuffle)
  REMAP_FUNC_OP(CompositeConstruct)
  REMAP_FUNC_OP(BinaryOp)
  REMAP_FUNC_OP(Store)
  REMAP_FUNC_OP(Load)
  REMAP_FUNC_OP(UnaryOp)
  REMAP_FUNC_OP(CompositeInsert)
  REMAP_FUNC_OP(BranchConditional)
  REMAP_FUNC_OP(Return)
  REMAP_FUNC_OP(ImageTexelPointer)
  REMAP_FUNC_OP(Select)
  REMAP_FUNC_OP(Switch)
  REMAP_FUNC_OP(CopyObject)
  REMAP_FUNC_OP(NonUniformUnaryOp)

private:
  ///< Whether in entry function wrapper, which will influence replace steps.
  bool inEntryFunctionWrapper;
  ///< Record inputs decoration info for location reassignment.
  llvm::DenseMap<SpirvInstruction *, SpirvDecoration *> m_inputDecorationMap;
  ///< Instruction replacement mapper.
  ///< For AccessChain and CompositeExtract, will only add extra index.
  llvm::DenseMap<SpirvInstruction *, SpirvInstruction *> m_instrReplaceMap;
  ///< Global declared structure type is special,
  ///< we won't redeclare/expand it more than once.
  llvm::SmallSet<const Type*, 4>                         m_expandedStructureType;
  ///< Context related helpers, will use to modify spv instruction stream.
  SpirvBuilder  &spirvBuilder;
  ASTContext    &astContext;
  SpirvModule   *currentMod;
  SpirvFunction *currentFunc;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_PERVERTEXINPUTVISITOR_H
