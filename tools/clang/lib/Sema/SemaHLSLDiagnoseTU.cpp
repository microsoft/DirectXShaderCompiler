///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SemaHLSLDiagnoseTU.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This file implements the Translation Unit Diagnose for HLSL.             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilFunctionProps.h"
#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/Support/Global.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "clang/Sema/SemaHLSL.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace clang;
using namespace llvm;
using namespace hlsl;

//
// This is similar to clang/Analysis/CallGraph, but the following differences
// motivate this:
//
// - track traversed vs. observed nodes explicitly
// - fully visit all reachable functions
// - merge graph visiting with checking for recursion
// - track global variables and types used (NYI)
//
namespace {
struct CallNode {
  FunctionDecl *CallerFn;
  ::llvm::SmallPtrSet<FunctionDecl *, 4> CalleeFns;
};
typedef ::llvm::DenseMap<FunctionDecl *, CallNode> CallNodes;
typedef ::llvm::SmallPtrSet<Decl *, 8> FnCallStack;
typedef ::llvm::SmallPtrSet<FunctionDecl *, 128> FunctionSet;
typedef ::llvm::SmallVector<FunctionDecl *, 32> PendingFunctions;
typedef ::llvm::DenseMap<FunctionDecl *, FunctionDecl *> FunctionMap;

// Returns the definition of a function.
// This serves two purposes - ignore built-in functions, and pick
// a single Decl * to be used in maps and sets.
FunctionDecl *getFunctionWithBody(FunctionDecl *F) {
  if (!F)
    return nullptr;
  if (F->doesThisDeclarationHaveABody())
    return F;
  F = F->getFirstDecl();
  for (auto &&Candidate : F->redecls()) {
    if (Candidate->doesThisDeclarationHaveABody()) {
      return Candidate;
    }
  }
  return nullptr;
}

// AST visitor that maintains visited and pending collections, as well
// as recording nodes of caller/callees.
class FnReferenceVisitor : public RecursiveASTVisitor<FnReferenceVisitor> {
private:
  CallNodes &m_callNodes;
  FunctionSet &m_visitedFunctions;
  PendingFunctions &m_pendingFunctions;
  FunctionDecl *m_source;
  CallNodes::iterator m_sourceIt;

public:
  FnReferenceVisitor(FunctionSet &visitedFunctions,
                     PendingFunctions &pendingFunctions, CallNodes &callNodes)
      : m_callNodes(callNodes), m_visitedFunctions(visitedFunctions),
        m_pendingFunctions(pendingFunctions) {}

  void setSourceFn(FunctionDecl *F) {
    F = getFunctionWithBody(F);
    m_source = F;
    m_sourceIt = m_callNodes.find(F);
  }

  bool VisitDeclRefExpr(DeclRefExpr *ref) {
    ValueDecl *valueDecl = ref->getDecl();
    RecordFunctionDecl(dyn_cast_or_null<FunctionDecl>(valueDecl));
    return true;
  }

  bool VisitCXXMemberCallExpr(CXXMemberCallExpr *callExpr) {
    RecordFunctionDecl(callExpr->getMethodDecl());
    return true;
  }

  void RecordFunctionDecl(FunctionDecl *funcDecl) {
    funcDecl = getFunctionWithBody(funcDecl);
    if (funcDecl) {
      if (m_sourceIt == m_callNodes.end()) {
        auto result = m_callNodes.insert(
            std::make_pair(m_source, CallNode{m_source, {}}));
        DXASSERT(result.second == true,
                 "else setSourceFn didn't assign m_sourceIt");
        m_sourceIt = result.first;
      }
      m_sourceIt->second.CalleeFns.insert(funcDecl);
      if (!m_visitedFunctions.count(funcDecl)) {
        m_pendingFunctions.push_back(funcDecl);
      }
    }
  }
};

// A call graph that can check for reachability and recursion efficiently.
class CallGraphWithRecurseGuard {
private:
  CallNodes m_callNodes;
  FunctionSet m_visitedFunctions;
  FunctionMap m_functionsCheckedForRecursion;

  FunctionDecl *CheckRecursion(FnCallStack &CallStack, FunctionDecl *D) {
    auto it = m_functionsCheckedForRecursion.find(D);
    if (it != m_functionsCheckedForRecursion.end())
      return it->second;
    if (CallStack.insert(D).second == false)
      return D;
    auto node = m_callNodes.find(D);
    if (node != m_callNodes.end()) {
      for (FunctionDecl *Callee : node->second.CalleeFns) {
        FunctionDecl *pResult = CheckRecursion(CallStack, Callee);
        if (pResult) {
          m_functionsCheckedForRecursion[D] = pResult;
          return pResult;
        }
      }
    }
    CallStack.erase(D);
    m_functionsCheckedForRecursion[D] = nullptr;
    return nullptr;
  }

public:
  void BuildForEntry(FunctionDecl *EntryFnDecl,
                     llvm::ArrayRef<VarDecl *> GlobalsWithInit) {
    DXASSERT_NOMSG(EntryFnDecl);
    EntryFnDecl = getFunctionWithBody(EntryFnDecl);
    PendingFunctions pendingFunctions;
    FnReferenceVisitor visitor(m_visitedFunctions, pendingFunctions,
                               m_callNodes);

    // First, traverse all initializers, then entry function.
    m_visitedFunctions.insert(EntryFnDecl);
    visitor.setSourceFn(EntryFnDecl);
    for (VarDecl *VD : GlobalsWithInit)
      visitor.TraverseDecl(VD);
    visitor.TraverseDecl(EntryFnDecl);

    while (!pendingFunctions.empty()) {
      FunctionDecl *pendingDecl = pendingFunctions.pop_back_val();
      if (m_visitedFunctions.insert(pendingDecl).second == true) {
        visitor.setSourceFn(pendingDecl);
        visitor.TraverseDecl(pendingDecl);
      }
    }
  }

  // return true if FD2 is reachable from FD1
  bool CheckReachability(FunctionDecl *FD1, FunctionDecl *FD2) {
    if (FD1 == FD2)
      return true;
    auto node = m_callNodes.find(FD1);
    if (node != m_callNodes.end()) {
      for (FunctionDecl *Callee : node->second.CalleeFns) {
        if (CheckReachability(Callee, FD2))
          return true;
      }
    }
    return false;
  }

  FunctionDecl *CheckRecursion(FunctionDecl *EntryFnDecl) {
    FnCallStack CallStack;
    EntryFnDecl = getFunctionWithBody(EntryFnDecl);
    return CheckRecursion(CallStack, EntryFnDecl);
  }

  const CallNodes &GetCallGraph() { return m_callNodes; }

  const FunctionSet GetVisitedFunctions() { return m_visitedFunctions; }

  void dump() const {
    llvm::dbgs() << "Call Nodes:\n";
    for (auto &node : m_callNodes) {
      llvm::dbgs() << node.first->getName().str().c_str() << " ["
                   << (void *)node.first << "]:\n";
      for (auto callee : node.second.CalleeFns) {
        llvm::dbgs() << "    " << callee->getName().str().c_str() << " ["
                     << (void *)callee << "]\n";
      }
    }
  }
};

struct NameLookup {
  FunctionDecl *Found;
  FunctionDecl *Other;
};

NameLookup GetSingleFunctionDeclByName(clang::Sema *self, StringRef Name,
                                       bool checkPatch) {
  auto DN = DeclarationName(&self->getASTContext().Idents.get(Name));
  FunctionDecl *pFoundDecl = nullptr;
  for (auto idIter = self->IdResolver.begin(DN), idEnd = self->IdResolver.end();
       idIter != idEnd; ++idIter) {
    FunctionDecl *pFnDecl = dyn_cast<FunctionDecl>(*idIter);
    if (!pFnDecl)
      continue;
    if (checkPatch &&
        !self->getASTContext().IsPatchConstantFunctionDecl(pFnDecl))
      continue;
    if (pFoundDecl) {
      return NameLookup{pFoundDecl, pFnDecl};
    }
    pFoundDecl = pFnDecl;
  }
  return NameLookup{pFoundDecl, nullptr};
}

bool IsTargetProfileLib6x(Sema &S) {
  // Remaining functions are exported only if target is 'lib_6_x'.
  const hlsl::ShaderModel *SM =
      hlsl::ShaderModel::GetByName(S.getLangOpts().HLSLProfile.c_str());
  bool isLib6x =
      SM->IsLib() && SM->GetMinor() == hlsl::ShaderModel::kOfflineMinor;
  return isLib6x;
}

bool IsExported(Sema *self, clang::FunctionDecl *FD,
                bool isDefaultLinkageExternal) {
  // Entry points are exported.
  if (FD->hasAttr<HLSLShaderAttr>())
    return true;

  // Internal linkage functions include functions marked 'static'.
  if (FD->getLinkageAndVisibility().getLinkage() == InternalLinkage)
    return false;

  // Explicit 'export' functions are exported.
  if (FD->hasAttr<HLSLExportAttr>())
    return true;

  return isDefaultLinkageExternal;
}

bool getDefaultLinkageExternal(clang::Sema *self) {
  const LangOptions &opts = self->getLangOpts();
  bool isDefaultLinkageExternal =
      opts.DefaultLinkage == DXIL::DefaultLinkage::External;
  if (opts.DefaultLinkage == DXIL::DefaultLinkage::Default &&
      !opts.ExportShadersOnly && IsTargetProfileLib6x(*self))
    isDefaultLinkageExternal = true;
  return isDefaultLinkageExternal;
}

std::vector<FunctionDecl *> GetAllExportedFDecls(clang::Sema *self) {
  // Add to the end, process from the beginning, to ensure AllExportedFDecls
  // will contain functions in decl order.
  std::vector<FunctionDecl *> AllExportedFDecls;

  std::deque<DeclContext *> Worklist;
  Worklist.push_back(self->getASTContext().getTranslationUnitDecl());
  while (Worklist.size()) {
    DeclContext *DC = Worklist.front();
    Worklist.pop_front();
    if (auto *FD = dyn_cast<FunctionDecl>(DC)) {
      AllExportedFDecls.push_back(FD);
    } else {
      for (auto *D : DC->decls()) {
        if (auto *FD = dyn_cast<FunctionDecl>(D)) {
          if (FD->hasBody() &&
              IsExported(self, FD, getDefaultLinkageExternal(self)))
            Worklist.push_back(FD);
        } else if (auto *DC2 = dyn_cast<DeclContext>(D)) {
          Worklist.push_back(DC2);
        }
      }
    }
  }

  return AllExportedFDecls;
}

void GatherGlobalsWithInitializers(
    DeclContext *DC, llvm::SmallVectorImpl<VarDecl *> &GlobalsWithInit,
    llvm::SmallVectorImpl<VarDecl *> &SubObjects) {
  for (auto *D : DC->decls()) {
    // Skip built-ins and function decls.
    if (D->isImplicit() || isa<FunctionDecl>(D))
      continue;
    if (auto *VD = dyn_cast<VarDecl>(D)) {
      // Add if user-defined static or groupshared global with initializer.
      if (VD->hasInit() && VD->hasGlobalStorage() &&
          (VD->getStorageClass() == SC_Static ||
           VD->hasAttr<HLSLGroupSharedAttr>())) {
        // Place subobjects in a separate collection.
        if (const RecordType *RT = VD->getType()->getAs<RecordType>()) {
          if (RT->getDecl()->hasAttr<HLSLSubObjectAttr>()) {
            SubObjects.push_back(VD);
            continue;
          }
        }
        GlobalsWithInit.push_back(VD);
      }
    } else if (auto *DC = dyn_cast<DeclContext>(D)) {
      // Recurse into DeclContexts like namespace, cbuffer, class/struct, etc.
      GatherGlobalsWithInitializers(DC, GlobalsWithInit, SubObjects);
    }
  }
}

// in the non-library case, this function will be run only once,
// but in the library case, this function will be run for each
// viable top-level function declaration by
// ValidateNoRecursionInTranslationUnit.
//  (viable as in, is exported)
clang::FunctionDecl *
ValidateNoRecursion(CallGraphWithRecurseGuard &callGraph,
                    clang::FunctionDecl *FD,
                    llvm::ArrayRef<VarDecl *> GlobalsWithInit) {
  // Validate that there is no recursion reachable by this function declaration
  // NOTE: the information gathered here could be used to bypass code generation
  // on functions that are unreachable (as an early form of dead code
  // elimination).
  if (FD) {
    callGraph.BuildForEntry(FD, GlobalsWithInit);
    return callGraph.CheckRecursion(FD);
  }
  return nullptr;
}

class HLSLReachableDiagnoseVisitor
    : public RecursiveASTVisitor<HLSLReachableDiagnoseVisitor> {
public:
  explicit HLSLReachableDiagnoseVisitor(
      Sema *S, const hlsl::ShaderModel *SM, DXIL::ShaderKind EntrySK,
      DXIL::NodeLaunchType NodeLaunchTy, const FunctionDecl *EntryDecl,
      llvm::SmallPtrSetImpl<CallExpr *> &DiagnosedCalls,
      llvm::SmallPtrSetImpl<DeclRefExpr *> &DeclAvailabilityChecked,
      llvm::SmallSet<SourceLocation, 16> &DiagnosedTypeLocs)
      : sema(S), SM(SM), EntrySK(EntrySK), NodeLaunchTy(NodeLaunchTy),
        EntryDecl(EntryDecl), DiagnosedCalls(DiagnosedCalls),
        DeclAvailabilityChecked(DeclAvailabilityChecked),
        DiagnosedTypeLocs(DiagnosedTypeLocs) {}

  bool VisitCallExpr(CallExpr *CE) {
    // Set flag if already diagnosed from another entry, allowing some
    // diagnostics to be skipped when they are not dependent on entry
    // properties.
    bool locallyVisited = DiagnosedCalls.count(CE) != 0;
    if (!locallyVisited)
      DiagnosedCalls.insert(CE);

    sema->DiagnoseReachableHLSLCall(CE, SM, EntrySK, NodeLaunchTy, EntryDecl,
                                    locallyVisited);
    return true;
  }

  bool VisitVarDecl(VarDecl *VD) {
    QualType VarType = VD->getType();
    if (const TemplateSpecializationType *TST =
            dyn_cast<TemplateSpecializationType>(VarType.getTypePtr())) {
      const TemplateDecl *TD = TST->getTemplateName().getAsTemplateDecl();
      if (!TD)
        return true;

      // verify this is a rayquery decl
      if (TD->getTemplatedDecl()->hasAttr<HLSLRayQueryObjectAttr>()) {
        if (TST->getNumArgs() == 1) {
          return true;
        }
        // now guaranteed 2 args
        const TemplateArgument &Arg2 = TST->getArg(1);
        Expr *Expr2 = Arg2.getAsExpr();
        llvm::APSInt Arg2val;
        Expr2->isIntegerConstantExpr(Arg2val, sema->getASTContext());

        const ShaderModel *SM = hlsl::ShaderModel::GetByName(
            sema->getLangOpts().HLSLProfile.c_str());

        if (Arg2val.getZExtValue() != 0 && !SM->IsSMAtLeast(6, 9)) {
          // if it's an integer literal, emit
          // warn_hlsl_rayquery_flags_disallowed
          if (Arg2.getKind() == TemplateArgument::Expression) {
            if (auto *castExpr = dyn_cast<ImplicitCastExpr>(
                    Arg2.getAsExpr()->IgnoreParens())) {
              // Now check if the sub-expression is a DeclRefExpr
              Expr *subExpr = castExpr->getSubExpr();
              if (auto *IL = dyn_cast<IntegerLiteral>(subExpr))
                sema->Diag(VD->getLocStart(),
                           diag::warn_hlsl_rayquery_flags_disallowed);
              return true;
            }
          }
        }
      }
    }
    return true;
  }

  bool VisitTypeLoc(TypeLoc TL) {
    // Diagnose availability for used type.
    if (AvailabilityAttr *AAttr = GetAvailabilityAttrOnce(TL)) {
      UnqualTypeLoc UTL = TL.getUnqualifiedLoc();
      DiagnoseAvailability(AAttr, TL.getType(), UTL.getLocStart());
    }

    return true;
  }

  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    // Diagnose availability for referenced decl.
    if (AvailabilityAttr *AAttr = GetAvailabilityAttrOnce(DRE)) {
      DiagnoseAvailability(AAttr, DRE->getDecl(), DRE->getExprLoc());
    }

    return true;
  }

  bool VisitMemberExpr(MemberExpr *ME) {
    // Diagnose availability for member function calls.
    if (AvailabilityAttr *AAttr = GetAvailabilityAttrOnce(ME)) {
      DiagnoseAvailability(AAttr, ME->getMemberDecl(), ME->getExprLoc());
    }

    return true;
  }

  AvailabilityAttr *GetAvailabilityAttrOnce(TypeLoc TL) {
    QualType Ty = TL.getType();
    CXXRecordDecl *RD = Ty->getAsCXXRecordDecl();
    if (!RD)
      return nullptr;
    AvailabilityAttr *AAttr = RD->getAttr<AvailabilityAttr>();
    if (!AAttr)
      return nullptr;
    // Skip redundant availability diagnostics for the same Type.
    // Use the end location to avoid diagnosing the same type multiple times.
    if (!DiagnosedTypeLocs.insert(TL.getEndLoc()).second)
      return nullptr;

    return AAttr;
  }

  AvailabilityAttr *GetAvailabilityAttrOnce(DeclRefExpr *DRE) {
    AvailabilityAttr *AAttr = DRE->getDecl()->getAttr<AvailabilityAttr>();
    if (!AAttr)
      return nullptr;
    // Skip redundant availability diagnostics for the same Decl.
    if (!DeclAvailabilityChecked.insert(DRE).second)
      return nullptr;

    return AAttr;
  }

  AvailabilityAttr *GetAvailabilityAttrOnce(MemberExpr *ME) {
    AvailabilityAttr *AAttr = ME->getMemberDecl()->getAttr<AvailabilityAttr>();
    if (!AAttr)
      return nullptr;
    // Skip redundant availability diagnostics for the same member.
    // Use the member location to track if we've already diagnosed this.
    if (!DiagnosedTypeLocs.insert(ME->getMemberLoc()).second)
      return nullptr;

    return AAttr;
  }

  bool CheckSMVersion(VersionTuple AAttrVT) {
    VersionTuple SMVT = VersionTuple(SM->GetMajor(), SM->GetMinor());
    return SMVT >= AAttrVT;
  }

  bool CheckObsoleted(VersionTuple ObsoletedVT) {
    if (ObsoletedVT.empty() || IsTargetProfileLib6x(*sema))
      return true;
    VersionTuple SMVT = VersionTuple(SM->GetMajor(), SM->GetMinor());
    return SMVT < ObsoletedVT;
  }

  void DiagnoseAvailability(AvailabilityAttr *AAttr, QualType Ty,
                            SourceLocation Loc) {
    VersionTuple AAttrVT = AAttr->getIntroduced();
    if (!CheckSMVersion(AAttrVT)) {
      sema->Diag(Loc, diag::warn_hlsl_builtin_type_unavailable)
          << Ty << SM->GetName() << AAttrVT.getAsString();
      return;
    }

    VersionTuple ObsoletedVT = AAttr->getObsoleted();
    if (!CheckObsoleted(ObsoletedVT))
      sema->Diag(Loc, diag::warn_hlsl_builtin_type_removed)
          << Ty << SM->GetName() << ObsoletedVT.getAsString();
  }

  void DiagnoseAvailability(AvailabilityAttr *AAttr, NamedDecl *ND,
                            SourceLocation Loc) {
    VersionTuple AAttrVT = AAttr->getIntroduced();
    if (!CheckSMVersion(AAttrVT)) {
      if (isa<FunctionDecl>(ND)) {
        sema->Diag(Loc, diag::warn_hlsl_intrinsic_in_wrong_shader_model)
            << ND->getQualifiedNameAsString() << EntryDecl
            << AAttrVT.getAsString();
        return;
      }

      sema->Diag(Loc, diag::warn_hlsl_builtin_constant_unavailable)
          << ND << SM->GetName() << AAttrVT.getAsString();
      return;
    }

    VersionTuple ObsoletedVT = AAttr->getObsoleted();
    if (CheckObsoleted(ObsoletedVT))
      return;

    if (isa<FunctionDecl>(ND)) {
      sema->Diag(Loc, diag::warn_hlsl_intrinsic_removed)
          << ND->getQualifiedNameAsString() << EntryDecl
          << ObsoletedVT.getAsString();
      return;
    }

    sema->Diag(Loc, diag::warn_hlsl_builtin_constant_removed)
        << ND << SM->GetName() << ObsoletedVT.getAsString();
  }

  clang::Sema *getSema() { return sema; }

private:
  clang::Sema *sema;
  const hlsl::ShaderModel *SM;
  DXIL::ShaderKind EntrySK;
  DXIL::NodeLaunchType NodeLaunchTy;
  const FunctionDecl *EntryDecl;
  llvm::SmallPtrSetImpl<CallExpr *> &DiagnosedCalls;
  llvm::SmallPtrSetImpl<DeclRefExpr *> &DeclAvailabilityChecked;
  llvm::SmallSet<SourceLocation, 16> &DiagnosedTypeLocs;
};

std::optional<uint32_t>
getFunctionInputPatchCount(const FunctionDecl *function) {
  for (const auto *param : function->params()) {
    if (!hlsl::IsHLSLInputPatchType(param->getType()))
      continue;
    return hlsl::GetHLSLInputPatchCount(param->getType());
  }

  return std::nullopt;
}

std::optional<uint32_t>
getFunctionOutputPatchCount(const FunctionDecl *function) {
  for (const auto *param : function->params()) {
    if (!hlsl::IsHLSLOutputPatchType(param->getType()))
      continue;
    return hlsl::GetHLSLOutputPatchCount(param->getType());
  }

  return std::nullopt;
}

std::optional<uint32_t>
getFunctionOutputControlPointsCount(const FunctionDecl *function) {
  if (const auto *Attr = function->getAttr<HLSLOutputControlPointsAttr>()) {
    return Attr->getCount();
  }
  return std::nullopt;
}

enum class BackwardDerivativeMismatchKind {
  None,
  Recursive,
  Method,
  ReturnType,
  ParameterCount,
  ParameterType,
  InputDirection,
  OutputDirection,
};

struct BackwardDerivativeMismatch {
  BackwardDerivativeMismatchKind Kind = BackwardDerivativeMismatchKind::None;
  unsigned ParameterIndex = 0;
  unsigned ExpectedParameterCount = 0;
  QualType ExpectedType;
};

bool isAutoDiffInactiveParameter(const ParmVarDecl *Parameter) {
  return Parameter->hasAttr<HLSLNoDiffAttr>() ||
         hlsl::IsHLSLResourceCarrierType(Parameter->getType());
}

BackwardDerivativeMismatch
getBackwardDerivativeMismatch(ASTContext &Context, const FunctionDecl *Primal,
                              const FunctionDecl *Derivative) {
  BackwardDerivativeMismatch Mismatch;
  if (Primal->getCanonicalDecl() == Derivative->getCanonicalDecl()) {
    Mismatch.Kind = BackwardDerivativeMismatchKind::Recursive;
    return Mismatch;
  }

  const auto *PrimalMethod = dyn_cast<CXXMethodDecl>(Primal);
  const auto *DerivativeMethod = dyn_cast<CXXMethodDecl>(Derivative);
  if ((PrimalMethod != nullptr) != (DerivativeMethod != nullptr) ||
      (PrimalMethod &&
       (PrimalMethod->getParent()->getCanonicalDecl() !=
            DerivativeMethod->getParent()->getCanonicalDecl() ||
        PrimalMethod->isStatic() != DerivativeMethod->isStatic()))) {
    Mismatch.Kind = BackwardDerivativeMismatchKind::Method;
    return Mismatch;
  }
  if (!Derivative->getReturnType()->isVoidType()) {
    Mismatch.Kind = BackwardDerivativeMismatchKind::ReturnType;
    return Mismatch;
  }

  unsigned ActiveParameters = 0;
  for (const ParmVarDecl *Parameter : Primal->parameters())
    ActiveParameters += !isAutoDiffInactiveParameter(Parameter);
  unsigned ExpectedParameters = Primal->getNumParams() + 1 + ActiveParameters;
  if (Derivative->getNumParams() != ExpectedParameters) {
    Mismatch.Kind = BackwardDerivativeMismatchKind::ParameterCount;
    Mismatch.ExpectedParameterCount = ExpectedParameters;
    return Mismatch;
  }

  for (unsigned I = 0; I < Primal->getNumParams(); ++I) {
    const ParmVarDecl *Input = Derivative->getParamDecl(I);
    if (!Context.hasSameType(Primal->getParamDecl(I)->getType(),
                             Input->getType().getNonReferenceType())) {
      Mismatch.Kind = BackwardDerivativeMismatchKind::ParameterType;
      Mismatch.ParameterIndex = I;
      Mismatch.ExpectedType = Primal->getParamDecl(I)->getType();
      return Mismatch;
    }
    if (Input->hasAttr<HLSLOutAttr>()) {
      Mismatch.Kind = BackwardDerivativeMismatchKind::InputDirection;
      Mismatch.ParameterIndex = I;
      return Mismatch;
    }
  }

  unsigned SeedIndex = Primal->getNumParams();
  const ParmVarDecl *Seed = Derivative->getParamDecl(SeedIndex);
  if (!Context.hasSameType(Primal->getReturnType(),
                           Seed->getType().getNonReferenceType())) {
    Mismatch.Kind = BackwardDerivativeMismatchKind::ParameterType;
    Mismatch.ParameterIndex = SeedIndex;
    Mismatch.ExpectedType = Primal->getReturnType();
    return Mismatch;
  }
  if (Seed->hasAttr<HLSLOutAttr>()) {
    Mismatch.Kind = BackwardDerivativeMismatchKind::InputDirection;
    Mismatch.ParameterIndex = SeedIndex;
    return Mismatch;
  }

  unsigned OutputIndex = SeedIndex + 1;
  for (const ParmVarDecl *Parameter : Primal->parameters()) {
    if (isAutoDiffInactiveParameter(Parameter))
      continue;
    const ParmVarDecl *Output = Derivative->getParamDecl(OutputIndex);
    if (!Context.hasSameType(Parameter->getType(),
                             Output->getType().getNonReferenceType())) {
      Mismatch.Kind = BackwardDerivativeMismatchKind::ParameterType;
      Mismatch.ParameterIndex = OutputIndex;
      Mismatch.ExpectedType = Parameter->getType();
      return Mismatch;
    }
    if (!Output->hasAttr<HLSLOutAttr>() || Output->hasAttr<HLSLInAttr>()) {
      Mismatch.Kind = BackwardDerivativeMismatchKind::OutputDirection;
      Mismatch.ParameterIndex = OutputIndex;
      return Mismatch;
    }
    ++OutputIndex;
  }
  return Mismatch;
}

void diagnoseBackwardDerivativeMismatch(
    Sema &S, const FunctionDecl *Primal, const FunctionDecl *Derivative,
    const HLSLBackwardDerivativeAttr *Attr,
    const BackwardDerivativeMismatch &Mismatch) {
  SourceLocation Location = Attr->getLocation();
  switch (Mismatch.Kind) {
  case BackwardDerivativeMismatchKind::None:
    return;
  case BackwardDerivativeMismatchKind::Recursive:
    S.Diag(Location, diag::err_hlsl_autodiff_association_recursive)
        << Primal->getName();
    return;
  case BackwardDerivativeMismatchKind::Method:
    S.Diag(Location, diag::err_hlsl_autodiff_association_method_mismatch)
        << Primal->getName();
    return;
  case BackwardDerivativeMismatchKind::ReturnType:
    S.Diag(Location, diag::err_hlsl_autodiff_pullback_return_type)
        << Derivative->getName();
    return;
  case BackwardDerivativeMismatchKind::ParameterCount:
    S.Diag(Location, diag::err_hlsl_autodiff_pullback_parameter_count)
        << Derivative->getName() << Mismatch.ExpectedParameterCount
        << Derivative->getNumParams();
    return;
  case BackwardDerivativeMismatchKind::ParameterType:
    S.Diag(Location, diag::err_hlsl_autodiff_pullback_parameter_type)
        << Derivative->getName() << (Mismatch.ParameterIndex + 1)
        << Mismatch.ExpectedType;
    return;
  case BackwardDerivativeMismatchKind::InputDirection:
    S.Diag(Location, diag::err_hlsl_autodiff_pullback_input)
        << Derivative->getName() << (Mismatch.ParameterIndex + 1);
    return;
  case BackwardDerivativeMismatchKind::OutputDirection:
    S.Diag(Location, diag::err_hlsl_autodiff_pullback_output)
        << Derivative->getName() << (Mismatch.ParameterIndex + 1);
    return;
  }
}

void diagnoseBackwardDerivativeAssociation(Sema &S,
                                           const FunctionDecl *Canonical) {
  SmallVector<std::pair<const FunctionDecl *,
                        const HLSLBackwardDerivativeAttr *>,
              2>
      Associations;
  for (const FunctionDecl *Redecl : Canonical->redecls())
    for (const auto *Attr :
         Redecl->specific_attrs<HLSLBackwardDerivativeAttr>())
      if (!Attr->isInherited())
        Associations.push_back({Redecl, Attr});
  if (Associations.empty())
    return;
  if (Associations.size() > 1) {
    S.Diag(Associations[1].second->getLocation(),
           diag::err_hlsl_autodiff_association_multiple)
        << Canonical->getName();
    return;
  }

  const FunctionDecl *Primal = Associations.front().first;
  const HLSLBackwardDerivativeAttr *Attr = Associations.front().second;
  SmallVector<const FunctionDecl *, 4> Candidates;
  SmallPtrSet<const FunctionDecl *, 4> SeenCandidates;
  for (const NamedDecl *Candidate : Primal->getDeclContext()->lookup(
           DeclarationName(Attr->getDerivative())))
    if (const auto *Function = dyn_cast<FunctionDecl>(Candidate))
      if (SeenCandidates.insert(Function->getCanonicalDecl()).second)
        Candidates.push_back(Function);

  if (Candidates.empty()) {
    S.Diag(Attr->getLocation(),
           diag::err_hlsl_autodiff_association_requires_function)
        << Attr->getSpelling();
    return;
  }

  SmallVector<const FunctionDecl *, 2> Matches;
  for (const FunctionDecl *Candidate : Candidates)
    if (getBackwardDerivativeMismatch(S.Context, Primal, Candidate).Kind ==
        BackwardDerivativeMismatchKind::None)
      Matches.push_back(Candidate);
  if (Matches.size() == 1)
    return;
  if (Matches.size() > 1) {
    S.Diag(Attr->getLocation(),
           diag::err_hlsl_autodiff_association_ambiguous)
        << Attr->getDerivative()->getName();
    return;
  }
  if (Candidates.size() == 1) {
    diagnoseBackwardDerivativeMismatch(
        S, Primal, Candidates.front(), Attr,
        getBackwardDerivativeMismatch(S.Context, Primal, Candidates.front()));
    return;
  }
  S.Diag(Attr->getLocation(),
         diag::err_hlsl_autodiff_association_no_matching_overload)
      << Attr->getDerivative()->getName() << Primal->getName();
}

void diagnoseBackwardDerivativeAssociations(Sema &S, DeclContext *Root) {
  SmallVector<DeclContext *, 16> Worklist(1, Root);
  SmallPtrSet<const FunctionDecl *, 32> SeenFunctions;
  while (!Worklist.empty()) {
    DeclContext *Context = Worklist.pop_back_val();
    for (Decl *D : Context->decls()) {
      if (const auto *Function = dyn_cast<FunctionDecl>(D)) {
        const FunctionDecl *Canonical = Function->getCanonicalDecl();
        if (SeenFunctions.insert(Canonical).second)
          diagnoseBackwardDerivativeAssociation(S, Canonical);
        continue;
      }
      if (auto *Child = dyn_cast<DeclContext>(D))
        Worklist.push_back(Child);
    }
  }
}

enum class PrimalSubstituteMismatchKind {
  None,
  Recursive,
  Method,
  ReturnType,
  ParameterCount,
  ParameterType,
  ParameterDirection,
  ParameterActivity,
};

struct PrimalSubstituteMismatch {
  PrimalSubstituteMismatchKind Kind = PrimalSubstituteMismatchKind::None;
  unsigned ParameterIndex = 0;
  unsigned ExpectedParameterCount = 0;
  QualType ExpectedType;
};

unsigned getParameterDirection(const ParmVarDecl *Parameter) {
  bool IsOut = Parameter->hasAttr<HLSLOutAttr>();
  bool IsIn = Parameter->hasAttr<HLSLInAttr>();
  if (IsOut && IsIn)
    return 2;
  return IsOut ? 1 : 0;
}

bool hasBackwardDifferentiationMode(const FunctionDecl *Function) {
  for (const FunctionDecl *Redecl : Function->redecls()) {
    if (const auto *Attr = Redecl->getAttr<HLSLAutoDiffAttr>())
      if (Attr->hasBackward())
        return true;
    if (Redecl->hasAttr<HLSLBackwardDerivativeAttr>())
      return true;
  }
  return false;
}

PrimalSubstituteMismatch
getPrimalSubstituteMismatch(ASTContext &Context, const FunctionDecl *Primal,
                            const FunctionDecl *Substitute) {
  PrimalSubstituteMismatch Mismatch;
  if (Primal->getCanonicalDecl() == Substitute->getCanonicalDecl()) {
    Mismatch.Kind = PrimalSubstituteMismatchKind::Recursive;
    return Mismatch;
  }

  const auto *PrimalMethod = dyn_cast<CXXMethodDecl>(Primal);
  const auto *SubstituteMethod = dyn_cast<CXXMethodDecl>(Substitute);
  if ((PrimalMethod != nullptr) != (SubstituteMethod != nullptr) ||
      (PrimalMethod &&
       (PrimalMethod->getParent()->getCanonicalDecl() !=
            SubstituteMethod->getParent()->getCanonicalDecl() ||
        PrimalMethod->isStatic() != SubstituteMethod->isStatic()))) {
    Mismatch.Kind = PrimalSubstituteMismatchKind::Method;
    return Mismatch;
  }
  if (!Context.hasSameType(Primal->getReturnType(),
                           Substitute->getReturnType())) {
    Mismatch.Kind = PrimalSubstituteMismatchKind::ReturnType;
    Mismatch.ExpectedType = Primal->getReturnType();
    return Mismatch;
  }
  if (Primal->getNumParams() != Substitute->getNumParams()) {
    Mismatch.Kind = PrimalSubstituteMismatchKind::ParameterCount;
    Mismatch.ExpectedParameterCount = Primal->getNumParams();
    return Mismatch;
  }

  for (unsigned I = 0; I < Primal->getNumParams(); ++I) {
    const ParmVarDecl *PrimalParameter = Primal->getParamDecl(I);
    const ParmVarDecl *SubstituteParameter = Substitute->getParamDecl(I);
    if (!Context.hasSameType(
            PrimalParameter->getType().getNonReferenceType(),
            SubstituteParameter->getType().getNonReferenceType())) {
      Mismatch.Kind = PrimalSubstituteMismatchKind::ParameterType;
      Mismatch.ParameterIndex = I;
      Mismatch.ExpectedType = PrimalParameter->getType().getNonReferenceType();
      return Mismatch;
    }
    if (getParameterDirection(PrimalParameter) !=
        getParameterDirection(SubstituteParameter)) {
      Mismatch.Kind = PrimalSubstituteMismatchKind::ParameterDirection;
      Mismatch.ParameterIndex = I;
      return Mismatch;
    }
    if (PrimalParameter->hasAttr<HLSLNoDiffAttr>() !=
        SubstituteParameter->hasAttr<HLSLNoDiffAttr>()) {
      Mismatch.Kind = PrimalSubstituteMismatchKind::ParameterActivity;
      Mismatch.ParameterIndex = I;
      return Mismatch;
    }
  }
  return Mismatch;
}

void diagnosePrimalSubstituteMismatch(
    Sema &S, const FunctionDecl *Primal, const FunctionDecl *Substitute,
    const HLSLPrimalSubstituteOfAttr *Attr,
    const PrimalSubstituteMismatch &Mismatch) {
  SourceLocation Location = Attr->getLocation();
  switch (Mismatch.Kind) {
  case PrimalSubstituteMismatchKind::None:
    return;
  case PrimalSubstituteMismatchKind::Recursive:
    S.Diag(Location, diag::err_hlsl_autodiff_substitute_recursive)
        << Primal->getName();
    return;
  case PrimalSubstituteMismatchKind::Method:
    S.Diag(Location, diag::err_hlsl_autodiff_substitute_method_mismatch)
        << Substitute->getName();
    return;
  case PrimalSubstituteMismatchKind::ReturnType:
    S.Diag(Location, diag::err_hlsl_autodiff_substitute_return_type)
        << Substitute->getName() << Mismatch.ExpectedType;
    return;
  case PrimalSubstituteMismatchKind::ParameterCount:
    S.Diag(Location, diag::err_hlsl_autodiff_substitute_parameter_count)
        << Substitute->getName() << Mismatch.ExpectedParameterCount
        << Substitute->getNumParams();
    return;
  case PrimalSubstituteMismatchKind::ParameterType:
    S.Diag(Location, diag::err_hlsl_autodiff_substitute_parameter_type)
        << Substitute->getName() << (Mismatch.ParameterIndex + 1)
        << Mismatch.ExpectedType;
    return;
  case PrimalSubstituteMismatchKind::ParameterDirection:
    S.Diag(Location, diag::err_hlsl_autodiff_substitute_parameter_direction)
        << Substitute->getName() << (Mismatch.ParameterIndex + 1);
    return;
  case PrimalSubstituteMismatchKind::ParameterActivity:
    S.Diag(Location, diag::err_hlsl_autodiff_substitute_parameter_activity)
        << Substitute->getName() << (Mismatch.ParameterIndex + 1);
    return;
  }
}

void diagnosePrimalSubstituteAssociations(Sema &S, DeclContext *Root) {
  struct Association {
    const FunctionDecl *Substitute;
    const HLSLPrimalSubstituteOfAttr *Attr;
  };
  SmallVector<DeclContext *, 16> Worklist(1, Root);
  SmallPtrSet<const FunctionDecl *, 32> SeenFunctions;
  SmallVector<Association, 16> Associations;
  while (!Worklist.empty()) {
    DeclContext *Context = Worklist.pop_back_val();
    for (Decl *D : Context->decls()) {
      if (const auto *Function = dyn_cast<FunctionDecl>(D)) {
        const FunctionDecl *Canonical = Function->getCanonicalDecl();
        if (!SeenFunctions.insert(Canonical).second)
          continue;
        for (const FunctionDecl *Redecl : Canonical->redecls())
          for (const auto *Attr :
               Redecl->specific_attrs<HLSLPrimalSubstituteOfAttr>())
            if (!Attr->isInherited())
              Associations.push_back({Redecl, Attr});
        continue;
      }
      if (auto *Child = dyn_cast<DeclContext>(D))
        Worklist.push_back(Child);
    }
  }

  DenseMap<const FunctionDecl *, const FunctionDecl *> Substitutes;
  DenseMap<const FunctionDecl *, const HLSLPrimalSubstituteOfAttr *> Attrs;
  for (const Association &Association : Associations) {
    const FunctionDecl *Substitute = Association.Substitute;
    const HLSLPrimalSubstituteOfAttr *Attr = Association.Attr;
    SmallVector<const FunctionDecl *, 4> Candidates;
    SmallPtrSet<const FunctionDecl *, 4> SeenCandidates;
    for (const NamedDecl *Candidate : Substitute->getDeclContext()->lookup(
             DeclarationName(Attr->getPrimal())))
      if (const auto *Function = dyn_cast<FunctionDecl>(Candidate))
        if (SeenCandidates.insert(Function->getCanonicalDecl()).second)
          Candidates.push_back(Function);

    if (Candidates.empty()) {
      S.Diag(Attr->getLocation(),
             diag::err_hlsl_autodiff_association_requires_function)
          << Attr->getSpelling();
      continue;
    }
    SmallVector<const FunctionDecl *, 2> Matches;
    for (const FunctionDecl *Candidate : Candidates)
      if (getPrimalSubstituteMismatch(S.Context, Candidate, Substitute).Kind ==
          PrimalSubstituteMismatchKind::None)
        Matches.push_back(Candidate);
    if (Matches.size() > 1) {
      S.Diag(Attr->getLocation(), diag::err_hlsl_autodiff_substitute_ambiguous)
          << Attr->getPrimal()->getName();
      continue;
    }
    if (Matches.empty()) {
      if (Candidates.size() == 1)
        diagnosePrimalSubstituteMismatch(
            S, Candidates.front(), Substitute, Attr,
            getPrimalSubstituteMismatch(S.Context, Candidates.front(),
                                        Substitute));
      else
        S.Diag(Attr->getLocation(),
               diag::err_hlsl_autodiff_substitute_no_matching_overload)
            << Attr->getPrimal()->getName() << Substitute->getName();
      continue;
    }

    const FunctionDecl *Primal = Matches.front()->getCanonicalDecl();
    const FunctionDecl *CanonicalSubstitute = Substitute->getCanonicalDecl();
    if (!hasBackwardDifferentiationMode(CanonicalSubstitute)) {
      S.Diag(Attr->getLocation(), diag::err_hlsl_autodiff_substitute_mode)
          << Substitute->getName();
      continue;
    }
    if (Substitutes.count(Primal)) {
      S.Diag(Attr->getLocation(), diag::err_hlsl_autodiff_substitute_multiple)
          << Primal->getName();
      continue;
    }
    Substitutes[Primal] = CanonicalSubstitute;
    Attrs[Primal] = Attr;
  }

  SmallPtrSet<const FunctionDecl *, 16> Complete;
  for (const auto &Entry : Substitutes) {
    SmallPtrSet<const FunctionDecl *, 8> Active;
    const FunctionDecl *Current = Entry.first;
    while (Substitutes.count(Current)) {
      if (!Active.insert(Current).second) {
        S.Diag(Attrs[Entry.first]->getLocation(),
               diag::err_hlsl_autodiff_substitute_cycle)
            << Entry.first->getName();
        break;
      }
      if (Complete.count(Current))
        break;
      Current = Substitutes[Current];
    }
    Complete.insert(Active.begin(), Active.end());
  }
}

} // namespace

void hlsl::DiagnoseTranslationUnit(clang::Sema *self) {
  DXASSERT_NOMSG(self != nullptr);

  // Don't bother with global validation if compilation has already failed.
  if (self->getDiagnostics().hasErrorOccurred()) {
    return;
  }

  diagnoseBackwardDerivativeAssociations(
      *self, self->getASTContext().getTranslationUnitDecl());
  if (self->getDiagnostics().hasErrorOccurred())
    return;
  diagnosePrimalSubstituteAssociations(
      *self, self->getASTContext().getTranslationUnitDecl());
  if (self->getDiagnostics().hasErrorOccurred())
    return;

  // Check RT shader if available for their payload use and match payload access
  // against availiable payload modifiers.
  // We have to do it late because we could have payload access in a called
  // function and have to check the callgraph if the root shader has the right
  // access rights to the payload structure.
  if (self->getLangOpts().IsHLSLLibrary) {
    if (self->getLangOpts().EnablePayloadAccessQualifiers) {
      ASTContext &ctx = self->getASTContext();
      TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();
      DiagnoseRaytracingPayloadAccess(*self, TU);
    }
  }

  // TODO: make these error 'real' errors rather than on-the-fly things
  // Validate that the entry point is available.
  DiagnosticsEngine &Diags = self->getDiagnostics();
  FunctionDecl *pEntryPointDecl = nullptr;
  std::vector<FunctionDecl *> FDeclsToCheck;
  if (self->getLangOpts().IsHLSLLibrary) {
    FDeclsToCheck = GetAllExportedFDecls(self);
  } else {
    const std::string &EntryPointName = self->getLangOpts().HLSLEntryFunction;
    if (!EntryPointName.empty()) {
      NameLookup NL = GetSingleFunctionDeclByName(self, EntryPointName,
                                                  /*checkPatch*/ false);
      if (NL.Found && NL.Other) {
        // NOTE: currently we cannot hit this codepath when CodeGen is enabled,
        // because CodeGenModule::getMangledName will mangle the entry point
        // name into the bare string, and so ambiguous points will produce an
        // error earlier on.
        unsigned id =
            Diags.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
                                  "ambiguous entry point function");
        Diags.Report(NL.Found->getSourceRange().getBegin(), id);
        Diags.Report(NL.Other->getLocation(), diag::note_previous_definition);
        return;
      }
      pEntryPointDecl = NL.Found;
      if (!pEntryPointDecl || !pEntryPointDecl->hasBody()) {
        unsigned id =
            Diags.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
                                  "missing entry point definition");
        Diags.Report(id);
        return;
      }
      FDeclsToCheck.push_back(NL.Found);
    }
  }

  const auto *shaderModel =
      hlsl::ShaderModel::GetByName(self->getLangOpts().HLSLProfile.c_str());

  llvm::SmallVector<VarDecl *, 16> GlobalsWithInit;
  llvm::SmallVector<VarDecl *, 16> SubObjects;
  std::set<FunctionDecl *> DiagnosedRecursiveDecls;
  llvm::SmallPtrSet<CallExpr *, 16> DiagnosedCalls;
  llvm::SmallPtrSet<DeclRefExpr *, 16> DeclAvailabilityChecked;
  llvm::SmallSet<SourceLocation, 16> DiagnosedTypeLocs;

  GatherGlobalsWithInitializers(self->getASTContext().getTranslationUnitDecl(),
                                GlobalsWithInit, SubObjects);

  if (shaderModel->GetKind() == DXIL::ShaderKind::Library) {
    DXIL::NodeLaunchType NodeLaunchTy = DXIL::NodeLaunchType::Invalid;
    HLSLReachableDiagnoseVisitor Visitor(
        self, shaderModel, shaderModel->GetKind(), NodeLaunchTy, nullptr,
        DiagnosedCalls, DeclAvailabilityChecked, DiagnosedTypeLocs);
    for (VarDecl *VD : SubObjects)
      Visitor.TraverseDecl(VD);
  }

  // for each FDecl, check for recursion
  for (FunctionDecl *FDecl : FDeclsToCheck) {
    CallGraphWithRecurseGuard callGraph;
    ArrayRef<VarDecl *> InitGlobals = {};
    // if entry function, include globals with initializers.
    if (FDecl->hasAttr<HLSLShaderAttr>())
      InitGlobals = GlobalsWithInit;
    FunctionDecl *result = ValidateNoRecursion(callGraph, FDecl, InitGlobals);

    if (result) {
      // don't emit duplicate diagnostics for the same recursive function
      // if A and B call recursive function C, only emit 1 diagnostic for C.
      if (DiagnosedRecursiveDecls.insert(result).second) {
        self->Diag(result->getSourceRange().getBegin(),
                   diag::err_hlsl_no_recursion)
            << FDecl->getQualifiedNameAsString()
            << result->getQualifiedNameAsString();
        self->Diag(result->getSourceRange().getBegin(),
                   diag::note_hlsl_no_recursion);
      }
    }

    FunctionDecl *pPatchFnDecl = nullptr;
    if (const HLSLPatchConstantFuncAttr *attr =
            FDecl->getAttr<HLSLPatchConstantFuncAttr>()) {
      NameLookup NL = GetSingleFunctionDeclByName(self, attr->getFunctionName(),
                                                  /*checkPatch*/ true);
      if (!NL.Found || !NL.Found->hasBody()) {
        self->Diag(attr->getLocation(),
                   diag::err_hlsl_missing_patch_constant_function)
            << attr->getFunctionName();
      }
      pPatchFnDecl = NL.Found;
    }

    if (pPatchFnDecl) {
      FunctionDecl *patchResult =
          ValidateNoRecursion(callGraph, pPatchFnDecl, GlobalsWithInit);

      // In this case, recursion was detected in the patch-constant function
      if (patchResult) {
        if (DiagnosedRecursiveDecls.insert(patchResult).second) {
          self->Diag(patchResult->getSourceRange().getBegin(),
                     diag::err_hlsl_no_recursion)
              << pPatchFnDecl->getQualifiedNameAsString()
              << patchResult->getQualifiedNameAsString();
          self->Diag(patchResult->getSourceRange().getBegin(),
                     diag::note_hlsl_no_recursion);
        }
      }

      // The patch function decl and the entry function decl should be
      // disconnected with respect to the call graph.
      // Only check this if neither function decl is recursive
      if (!result && !patchResult) {
        if (callGraph.CheckReachability(pPatchFnDecl, FDecl)) {
          self->Diag(FDecl->getSourceRange().getBegin(),
                     diag::err_hlsl_patch_reachability_not_allowed)
              << 1 << FDecl->getName() << 0 << pPatchFnDecl->getName();
        }
        if (callGraph.CheckReachability(FDecl, pPatchFnDecl)) {
          self->Diag(FDecl->getSourceRange().getBegin(),
                     diag::err_hlsl_patch_reachability_not_allowed)
              << 0 << pPatchFnDecl->getName() << 1 << FDecl->getName();
        }
      }

      // Input/Output control point validation.
      {
        auto hullPatchCount = getFunctionInputPatchCount(pPatchFnDecl);
        auto functionPatchCount = getFunctionInputPatchCount(FDecl);
        if (hullPatchCount.has_value() && functionPatchCount.has_value() &&
            hullPatchCount.value() != functionPatchCount.value()) {
          self->Diag(pPatchFnDecl->getSourceRange().getBegin(),
                     diag::err_hlsl_patch_size_mismatch)
              << "input" << functionPatchCount.value()
              << hullPatchCount.value();
        }
      }
      {
        auto hullPatchCount = getFunctionOutputPatchCount(pPatchFnDecl);
        auto functionPatchCount = getFunctionOutputControlPointsCount(FDecl);
        if (hullPatchCount.has_value() && functionPatchCount.has_value() &&
            hullPatchCount.value() != functionPatchCount.value()) {
          self->Diag(pPatchFnDecl->getSourceRange().getBegin(),
                     diag::err_hlsl_patch_size_mismatch)
              << "output" << functionPatchCount.value()
              << hullPatchCount.value();
        }
      }
      for (const auto *param : pPatchFnDecl->params()) {
        const TypeDiagContext ParamDiagContext =
            TypeDiagContext::PatchConstantFunctionParameters;
        DiagnoseTypeElements(*self, param->getLocation(), param->getType(),
                             ParamDiagContext, ParamDiagContext);
      }

      const TypeDiagContext ReturnDiagContext =
          TypeDiagContext::PatchConstantFunctionReturnType;
      DiagnoseTypeElements(*self, pPatchFnDecl->getLocation(),
                           pPatchFnDecl->getReturnType(), ReturnDiagContext,
                           ReturnDiagContext);
    }
    DXIL::ShaderKind EntrySK = shaderModel->GetKind();
    DXIL::NodeLaunchType NodeLaunchTy = DXIL::NodeLaunchType::Invalid;
    if (EntrySK == DXIL::ShaderKind::Library) {
      // For library, check if the exported function is entry with shader
      // attribute.
      if (const auto *Attr = FDecl->getAttr<clang::HLSLShaderAttr>())
        EntrySK = ShaderModel::KindFromFullName(Attr->getStage());
      if (EntrySK == DXIL::ShaderKind::Node) {
        if (const auto *pAttr = FDecl->getAttr<HLSLNodeLaunchAttr>())
          NodeLaunchTy =
              ShaderModel::NodeLaunchTypeFromName(pAttr->getLaunchType());
        else
          NodeLaunchTy = DXIL::NodeLaunchType::Broadcasting;
      }
    }

    // lib_6_x is an offline linking target, which allows exported functions to
    // perform actions that would otherwise be disallowed, as long as these
    // actions are either inlined into entry points where they are allowed, or
    // eliminated by the time the final library is linked to a runtime supported
    // shader model. Therefore, skip reachable call diagnostics for non-entry
    // points.
    if (EntrySK == DXIL::ShaderKind::Library && IsTargetProfileLib6x(*self))
      continue;

    // Work Graphs (node shaders) were obsoleted in shader model 6.10.
    // Declaring a node shader when targeting 6.10 or above is an error,
    // regardless of whether any node record types are actually used.
    if (EntrySK == DXIL::ShaderKind::Node && shaderModel->IsSMAtLeast(6, 10) &&
        !IsTargetProfileLib6x(*self)) {
      self->Diag(FDecl->getLocation(), diag::warn_hlsl_node_shader_removed)
          << shaderModel->GetName();
    }

    // Visit all visited functions in call graph to collect illegal intrinsic
    // calls.
    HLSLReachableDiagnoseVisitor Visitor(
        self, shaderModel, EntrySK, NodeLaunchTy, FDecl, DiagnosedCalls,
        DeclAvailabilityChecked, DiagnosedTypeLocs);
    // Visit globals with initializers when processing entry point.
    for (VarDecl *VD : InitGlobals)
      Visitor.TraverseDecl(VD);
    for (FunctionDecl *FD : callGraph.GetVisitedFunctions())
      Visitor.TraverseDecl(FD);
  }
}
