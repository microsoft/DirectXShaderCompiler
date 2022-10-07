//===--- StmtDslPrinter.cpp - Printing implementation for Stmt ASTs ----------===//
//
// This file implements the Stmt::dumpPretty/Stmt::printPretty methods, which
// pretty print the AST back out to MetaDSL[C] code.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Basic/CharInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Format.h"
#include "clang/Sema/SemaHLSL.h" // HLSL Change
using namespace clang;

//===----------------------------------------------------------------------===//
// StmtDslPrinter Visitor
//===----------------------------------------------------------------------===//

namespace  {
  class StmtDslPrinter : public StmtVisitor<StmtDslPrinter> {
    raw_ostream &OS;
    unsigned IndentLevel;
    clang::PrinterHelper* Helper;
    PrintingPolicy Policy;

  public:
    StmtDslPrinter(raw_ostream &os, PrinterHelper* helper,
                const PrintingPolicy &Policy,
                unsigned Indentation = 0)
      : OS(os), IndentLevel(Indentation), Helper(helper), Policy(Policy) {}

    void PrintStmt(Stmt *S) {
      PrintStmt(S, Policy.Indentation);
    }

    void PrintStmt(Stmt *S, int SubIndent) {
      IndentLevel += SubIndent;
      if (S && isa<Expr>(S)) {
        // If this is an expr used in a stmt context, indent and newline it.
        Indent();
        Visit(S);
        OS << ";\n";
      } else if (S) {
        Visit(S);
      } else {
        Indent() << "<<<NULL STATEMENT>>>\n";
      }
      IndentLevel -= SubIndent;
    }

    void PrintRawCompoundStmt(CompoundStmt *S);
    void PrintRawDecl(Decl *D);
    void PrintRawDeclStmt(const DeclStmt *S);
    void PrintRawIfStmt(IfStmt *If);
    void PrintRawCXXCatchStmt(CXXCatchStmt *Catch);
    void PrintCallArgs(CallExpr *E);
    void PrintRawSEHExceptHandler(SEHExceptStmt *S);
    void PrintRawSEHFinallyStmt(SEHFinallyStmt *S);
    void PrintOMPExecutableDirective(OMPExecutableDirective *S);

    void PrintExpr(Expr *E) {
      if (E)
        Visit(E);
      else
        OS << "<null expr>";
    }

    raw_ostream &Indent(int Delta = 0) {
      for (int i = 0, e = IndentLevel+Delta; i < e; ++i)
        OS << "  ";
      return OS;
    }

    void Visit(Stmt* S) {
      if (Helper && Helper->handledStmt(S,OS))
          return;
      else StmtVisitor<StmtDslPrinter>::Visit(S);
    }

    void VisitStmt(Stmt *Node) LLVM_ATTRIBUTE_UNUSED {
      Indent() << "<<unknown stmt type>>\n";
    }
    void VisitExpr(Expr *Node) LLVM_ATTRIBUTE_UNUSED {
      OS << "<<unknown expr type>>";
    }
    void VisitCXXNamedCastExpr(CXXNamedCastExpr *Node);

#define ABSTRACT_STMT(CLASS)
#define STMT(CLASS, PARENT) \
    void Visit##CLASS(CLASS *Node);
#include "clang/AST/StmtNodes.inc"
  };
}

//===----------------------------------------------------------------------===//
//  Stmt printing methods.
//===----------------------------------------------------------------------===//

/// PrintRawCompoundStmt - Print a compound stmt without indenting the {, and
/// with no newline after the }.
void StmtDslPrinter::PrintRawCompoundStmt(CompoundStmt *Node) {
  OS << "{\n";
  for (auto *I : Node->body())
    PrintStmt(I);

  Indent() << "}";
}

void StmtDslPrinter::PrintRawDecl(Decl *D) {
  D->print(OS, Policy, IndentLevel);
}

void StmtDslPrinter::PrintRawDeclStmt(const DeclStmt *S) {
  SmallVector<Decl*, 2> Decls(S->decls());
  Decl::printGroup(Decls.data(), Decls.size(), OS, Policy, IndentLevel);
}

void StmtDslPrinter::VisitNullStmt(NullStmt *Node) {
  Indent() << ";\n";
}

// HLSL Change: adding support for HLSL discard stmt
void StmtDslPrinter::VisitDiscardStmt(DiscardStmt *Node) {
  Indent();
  OS << "discard;\n";
}

void StmtDslPrinter::VisitDeclStmt(DeclStmt *Node) {
  Indent();
  PrintRawDeclStmt(Node);
  OS << ";\n";
}

void StmtDslPrinter::VisitCompoundStmt(CompoundStmt *Node) {
  Indent();
  PrintRawCompoundStmt(Node);
  OS << "\n";
}

void StmtDslPrinter::VisitCaseStmt(CaseStmt *Node) {
  Indent(-1) << "case ";
  PrintExpr(Node->getLHS());
  if (Node->getRHS()) {
    OS << " ... ";
    PrintExpr(Node->getRHS());
  }
  OS << ":\n";

  PrintStmt(Node->getSubStmt(), 0);
}

void StmtDslPrinter::VisitDefaultStmt(DefaultStmt *Node) {
  Indent(-1) << "default:\n";
  PrintStmt(Node->getSubStmt(), 0);
}

void StmtDslPrinter::VisitLabelStmt(LabelStmt *Node) {
  Indent(-1) << Node->getName() << ":\n";
  PrintStmt(Node->getSubStmt(), 0);
}

void StmtDslPrinter::VisitAttributedStmt(AttributedStmt *Node) {
  // HLSL Change Begin
  if (Policy.LangOpts.HLSL) {
    for (ArrayRef<const Attr*>::reverse_iterator it = Node->getAttrs().rbegin(),
      end = Node->getAttrs().rend(); it != end; ++it) {
      hlsl::CustomPrintHLSLAttr((*it), OS, Policy, IndentLevel);
    }

    PrintStmt(Node->getSubStmt(), 0);
    return;
  }
  // HLSL Change End

  for (const auto *Attr : Node->getAttrs()) {
    Attr->printPretty(OS, Policy);
  }

  PrintStmt(Node->getSubStmt(), 0);
}

void StmtDslPrinter::PrintRawIfStmt(IfStmt *If) {
  OS << "if (";
  if (const DeclStmt *DS = If->getConditionVariableDeclStmt())
    PrintRawDeclStmt(DS);
  else
    PrintExpr(If->getCond());
  OS << ')';

  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(If->getThen())) {
    OS << ' ';
    PrintRawCompoundStmt(CS);
    OS << (If->getElse() ? " " : ";\n");
  } else {
    OS << '\n';
    PrintStmt(If->getThen());
    if (If->getElse()) Indent();
  }

  if (Stmt *Else = If->getElse()) {
    OS << "else";

    if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Else)) {
      OS << ' ';
      PrintRawCompoundStmt(CS);
      OS << ';\n';
    } else if (IfStmt *ElseIf = dyn_cast<IfStmt>(Else)) {
      OS << ' ';
      PrintRawIfStmt(ElseIf);
    } else {
      OS << '\n';
      PrintStmt(If->getElse());
    }
  }
}

void StmtDslPrinter::VisitIfStmt(IfStmt *If) {
  Indent();
  PrintRawIfStmt(If);
}

void StmtDslPrinter::VisitSwitchStmt(SwitchStmt *Node) {
  Indent() << "switch (";
  if (const DeclStmt *DS = Node->getConditionVariableDeclStmt())
    PrintRawDeclStmt(DS);
  else
    PrintExpr(Node->getCond());
  OS << ")";

  // Pretty print compoundstmt bodies (very common).
  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Node->getBody())) {
    OS << " ";
    PrintRawCompoundStmt(CS);
    OS << ";\n";
  } else {
    OS << "\n";
    PrintStmt(Node->getBody());
  }
}

void StmtDslPrinter::VisitWhileStmt(WhileStmt *Node) {
  Indent() << "while (";
  if (const DeclStmt *DS = Node->getConditionVariableDeclStmt())
    PrintRawDeclStmt(DS);
  else
    PrintExpr(Node->getCond());
  OS << ")\n";
  IndentLevel = IndentLevel - 1; // HLSL Change - TODO: Find the root cause of this issue.
  PrintStmt(Node->getBody());
  IndentLevel = IndentLevel + 1; // HLSL Change  
}

void StmtDslPrinter::VisitDoStmt(DoStmt *Node) {
  Indent() << "do ";
  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Node->getBody())) {
    PrintRawCompoundStmt(CS);
    OS << " ";
  } else {
    OS << "\n";
    PrintStmt(Node->getBody());
    Indent();
  }

  OS << "while (";
  PrintExpr(Node->getCond());
  OS << ");\n";
}

void StmtDslPrinter::VisitForStmt(ForStmt *Node) {
  Indent() << "for (";
  if (Node->getInit()) {
    if (DeclStmt *DS = dyn_cast<DeclStmt>(Node->getInit()))
      PrintRawDeclStmt(DS);
    else
      PrintExpr(cast<Expr>(Node->getInit()));
  }
  OS << ";";
  if (Node->getCond()) {
    OS << " ";
    PrintExpr(Node->getCond());
  }
  OS << ";";
  if (Node->getInc()) {
    OS << " ";
    PrintExpr(Node->getInc());
  }
  OS << ") ";

  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Node->getBody())) {
    PrintRawCompoundStmt(CS);
    OS << ";\n";
  } else {
    OS << "\n";
    PrintStmt(Node->getBody());
  }
}

void StmtDslPrinter::VisitObjCForCollectionStmt(ObjCForCollectionStmt *Node) {
  Indent() << "for (";
  if (DeclStmt *DS = dyn_cast<DeclStmt>(Node->getElement()))
    PrintRawDeclStmt(DS);
  else
    PrintExpr(cast<Expr>(Node->getElement()));
  OS << " in ";
  PrintExpr(Node->getCollection());
  OS << ") ";

  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Node->getBody())) {
    PrintRawCompoundStmt(CS);
    OS << ";\n";
  } else {
    OS << "\n";
    PrintStmt(Node->getBody());
  }
}

void StmtDslPrinter::VisitCXXForRangeStmt(CXXForRangeStmt *Node) {
  Indent() << "for (";
  PrintingPolicy SubPolicy(Policy);
  SubPolicy.SuppressInitializers = true;
  Node->getLoopVariable()->print(OS, SubPolicy, IndentLevel);
  OS << " : ";
  PrintExpr(Node->getRangeInit());
  OS << ") {\n";
  PrintStmt(Node->getBody());
  Indent() << "};";
  if (Policy.IncludeNewlines) OS << "\n";
}

void StmtDslPrinter::VisitMSDependentExistsStmt(MSDependentExistsStmt *Node) {
  Indent();
  if (Node->isIfExists())
    OS << "__if_exists (";
  else
    OS << "__if_not_exists (";
  
  if (NestedNameSpecifier *Qualifier
        = Node->getQualifierLoc().getNestedNameSpecifier())
    Qualifier->print(OS, Policy);
  
  OS << Node->getNameInfo() << ") ";
  
  PrintRawCompoundStmt(Node->getSubStmt());
}

void StmtDslPrinter::VisitGotoStmt(GotoStmt *Node) {
  Indent() << "goto " << Node->getLabel()->getName() << ";";
  if (Policy.IncludeNewlines) OS << "\n";
}

void StmtDslPrinter::VisitIndirectGotoStmt(IndirectGotoStmt *Node) {
  Indent() << "goto *";
  PrintExpr(Node->getTarget());
  OS << ";";
  if (Policy.IncludeNewlines) OS << "\n";
}

void StmtDslPrinter::VisitContinueStmt(ContinueStmt *Node) {
  Indent() << "continue;";
  if (Policy.IncludeNewlines) OS << "\n";
}

void StmtDslPrinter::VisitBreakStmt(BreakStmt *Node) {
  Indent() << "break;";
  if (Policy.IncludeNewlines) OS << "\n";
}


void StmtDslPrinter::VisitReturnStmt(ReturnStmt *Node) {
  Indent() << "return";
  if (Node->getRetValue()) {
    OS << " <- ";
    PrintExpr(Node->getRetValue());
  } else {
    OS << "()";
  }
  OS << ";";
  if (Policy.IncludeNewlines) OS << "\n";
}


void StmtDslPrinter::VisitGCCAsmStmt(GCCAsmStmt *Node) {
  Indent() << "asm ";

  if (Node->isVolatile())
    OS << "volatile ";

  OS << "(";
  VisitStringLiteral(Node->getAsmString());

  // Outputs
  if (Node->getNumOutputs() != 0 || Node->getNumInputs() != 0 ||
      Node->getNumClobbers() != 0)
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumOutputs(); i != e; ++i) {
    if (i != 0)
      OS << ", ";

    if (!Node->getOutputName(i).empty()) {
      OS << '[';
      OS << Node->getOutputName(i);
      OS << "] ";
    }

    VisitStringLiteral(Node->getOutputConstraintLiteral(i));
    OS << " (";
    Visit(Node->getOutputExpr(i));
    OS << ")";
  }

  // Inputs
  if (Node->getNumInputs() != 0 || Node->getNumClobbers() != 0)
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumInputs(); i != e; ++i) {
    if (i != 0)
      OS << ", ";

    if (!Node->getInputName(i).empty()) {
      OS << '[';
      OS << Node->getInputName(i);
      OS << "] ";
    }

    VisitStringLiteral(Node->getInputConstraintLiteral(i));
    OS << " (";
    Visit(Node->getInputExpr(i));
    OS << ")";
  }

  // Clobbers
  if (Node->getNumClobbers() != 0)
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumClobbers(); i != e; ++i) {
    if (i != 0)
      OS << ", ";

    VisitStringLiteral(Node->getClobberStringLiteral(i));
  }

  OS << ");";
  if (Policy.IncludeNewlines) OS << "\n";
}

void StmtDslPrinter::VisitMSAsmStmt(MSAsmStmt *Node) {
  // FIXME: Implement MS style inline asm statement printer.
  Indent() << "__asm ";
  if (Node->hasBraces())
    OS << "{\n";
  OS << Node->getAsmString() << "\n";
  if (Node->hasBraces())
    Indent() << "}\n";
}

void StmtDslPrinter::VisitCapturedStmt(CapturedStmt *Node) {
  PrintStmt(Node->getCapturedDecl()->getBody());
}

void StmtDslPrinter::VisitObjCAtTryStmt(ObjCAtTryStmt *Node) {
  Indent() << "@try";
  if (CompoundStmt *TS = dyn_cast<CompoundStmt>(Node->getTryBody())) {
    PrintRawCompoundStmt(TS);
    OS << "\n";
  }

  for (unsigned I = 0, N = Node->getNumCatchStmts(); I != N; ++I) {
    ObjCAtCatchStmt *catchStmt = Node->getCatchStmt(I);
    Indent() << "@catch(";
    if (catchStmt->getCatchParamDecl()) {
      if (Decl *DS = catchStmt->getCatchParamDecl())
        PrintRawDecl(DS);
    }
    OS << ")";
    if (CompoundStmt *CS = dyn_cast<CompoundStmt>(catchStmt->getCatchBody())) {
      PrintRawCompoundStmt(CS);
      OS << "\n";
    }
  }

  if (ObjCAtFinallyStmt *FS = static_cast<ObjCAtFinallyStmt *>(
        Node->getFinallyStmt())) {
    Indent() << "@finally";
    PrintRawCompoundStmt(dyn_cast<CompoundStmt>(FS->getFinallyBody()));
    OS << "\n";
  }
}

void StmtDslPrinter::VisitObjCAtFinallyStmt(ObjCAtFinallyStmt *Node) {
}

void StmtDslPrinter::VisitObjCAtCatchStmt (ObjCAtCatchStmt *Node) {
  Indent() << "@catch (...) { /* todo */ } \n";
}

void StmtDslPrinter::VisitObjCAtThrowStmt(ObjCAtThrowStmt *Node) {
  Indent() << "@throw";
  if (Node->getThrowExpr()) {
    OS << " ";
    PrintExpr(Node->getThrowExpr());
  }
  OS << ";\n";
}

void StmtDslPrinter::VisitObjCAtSynchronizedStmt(ObjCAtSynchronizedStmt *Node) {
  Indent() << "@synchronized (";
  PrintExpr(Node->getSynchExpr());
  OS << ")";
  PrintRawCompoundStmt(Node->getSynchBody());
  OS << "\n";
}

void StmtDslPrinter::VisitObjCAutoreleasePoolStmt(ObjCAutoreleasePoolStmt *Node) {
  Indent() << "@autoreleasepool";
  PrintRawCompoundStmt(dyn_cast<CompoundStmt>(Node->getSubStmt()));
  OS << "\n";
}

void StmtDslPrinter::PrintRawCXXCatchStmt(CXXCatchStmt *Node) {
  OS << "catch (";
  if (Decl *ExDecl = Node->getExceptionDecl())
    PrintRawDecl(ExDecl);
  else
    OS << "...";
  OS << ") ";
  PrintRawCompoundStmt(cast<CompoundStmt>(Node->getHandlerBlock()));
}

void StmtDslPrinter::VisitCXXCatchStmt(CXXCatchStmt *Node) {
  Indent();
  PrintRawCXXCatchStmt(Node);
  OS << "\n";
}

void StmtDslPrinter::VisitCXXTryStmt(CXXTryStmt *Node) {
  Indent() << "try ";
  PrintRawCompoundStmt(Node->getTryBlock());
  for (unsigned i = 0, e = Node->getNumHandlers(); i < e; ++i) {
    OS << " ";
    PrintRawCXXCatchStmt(Node->getHandler(i));
  }
  OS << "\n";
}

void StmtDslPrinter::VisitSEHTryStmt(SEHTryStmt *Node) {
  Indent() << (Node->getIsCXXTry() ? "try " : "__try ");
  PrintRawCompoundStmt(Node->getTryBlock());
  SEHExceptStmt *E = Node->getExceptHandler();
  SEHFinallyStmt *F = Node->getFinallyHandler();
  if(E)
    PrintRawSEHExceptHandler(E);
  else {
    assert(F && "Must have a finally block...");
    PrintRawSEHFinallyStmt(F);
  }
  OS << "\n";
}

void StmtDslPrinter::PrintRawSEHFinallyStmt(SEHFinallyStmt *Node) {
  OS << "__finally ";
  PrintRawCompoundStmt(Node->getBlock());
  OS << "\n";
}

void StmtDslPrinter::PrintRawSEHExceptHandler(SEHExceptStmt *Node) {
  OS << "__except (";
  VisitExpr(Node->getFilterExpr());
  OS << ")\n";
  PrintRawCompoundStmt(Node->getBlock());
  OS << "\n";
}

void StmtDslPrinter::VisitSEHExceptStmt(SEHExceptStmt *Node) {
  Indent();
  PrintRawSEHExceptHandler(Node);
  OS << "\n";
}

void StmtDslPrinter::VisitSEHFinallyStmt(SEHFinallyStmt *Node) {
  Indent();
  PrintRawSEHFinallyStmt(Node);
  OS << "\n";
}

void StmtDslPrinter::VisitSEHLeaveStmt(SEHLeaveStmt *Node) {
  Indent() << "__leave;";
  if (Policy.IncludeNewlines) OS << "\n";
}

//===----------------------------------------------------------------------===//
//  OpenMP clauses printing methods
//===----------------------------------------------------------------------===//

namespace {
class OMPClausePrinter : public OMPClauseVisitor<OMPClausePrinter> {
  raw_ostream &OS;
  const PrintingPolicy &Policy;
  /// \brief Process clauses with list of variables.
  template <typename T>
  void VisitOMPClauseList(T *Node, char StartSym);
public:
  OMPClausePrinter(raw_ostream &OS, const PrintingPolicy &Policy)
    : OS(OS), Policy(Policy) { }
#define OPENMP_CLAUSE(Name, Class)                              \
  void Visit##Class(Class *S);
#include "clang/Basic/OpenMPKinds.def"
};

void OMPClausePrinter::VisitOMPIfClause(OMPIfClause *Node) {
  OS << "if(";
  Node->getCondition()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPFinalClause(OMPFinalClause *Node) {
  OS << "final(";
  Node->getCondition()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPNumThreadsClause(OMPNumThreadsClause *Node) {
  OS << "num_threads(";
  Node->getNumThreads()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPSafelenClause(OMPSafelenClause *Node) {
  OS << "safelen(";
  Node->getSafelen()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPCollapseClause(OMPCollapseClause *Node) {
  OS << "collapse(";
  Node->getNumForLoops()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPDefaultClause(OMPDefaultClause *Node) {
  OS << "default("
     << getOpenMPSimpleClauseTypeName(OMPC_default, Node->getDefaultKind())
     << ")";
}

void OMPClausePrinter::VisitOMPProcBindClause(OMPProcBindClause *Node) {
  OS << "proc_bind("
     << getOpenMPSimpleClauseTypeName(OMPC_proc_bind, Node->getProcBindKind())
     << ")";
}

void OMPClausePrinter::VisitOMPScheduleClause(OMPScheduleClause *Node) {
  OS << "schedule("
     << getOpenMPSimpleClauseTypeName(OMPC_schedule, Node->getScheduleKind());
  if (Node->getChunkSize()) {
    OS << ", ";
    Node->getChunkSize()->printPretty(OS, nullptr, Policy);
  }
  OS << ")";
}

void OMPClausePrinter::VisitOMPOrderedClause(OMPOrderedClause *) {
  OS << "ordered";
}

void OMPClausePrinter::VisitOMPNowaitClause(OMPNowaitClause *) {
  OS << "nowait";
}

void OMPClausePrinter::VisitOMPUntiedClause(OMPUntiedClause *) {
  OS << "untied";
}

void OMPClausePrinter::VisitOMPMergeableClause(OMPMergeableClause *) {
  OS << "mergeable";
}

void OMPClausePrinter::VisitOMPReadClause(OMPReadClause *) { OS << "read"; }

void OMPClausePrinter::VisitOMPWriteClause(OMPWriteClause *) { OS << "write"; }

void OMPClausePrinter::VisitOMPUpdateClause(OMPUpdateClause *) {
  OS << "update";
}

void OMPClausePrinter::VisitOMPCaptureClause(OMPCaptureClause *) {
  OS << "capture";
}

void OMPClausePrinter::VisitOMPSeqCstClause(OMPSeqCstClause *) {
  OS << "seq_cst";
}

template<typename T>
void OMPClausePrinter::VisitOMPClauseList(T *Node, char StartSym) {
  for (typename T::varlist_iterator I = Node->varlist_begin(),
                                    E = Node->varlist_end();
         I != E; ++I) {
    assert(*I && "Expected non-null Stmt");
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*I)) {
      OS << (I == Node->varlist_begin() ? StartSym : ',');
      cast<NamedDecl>(DRE->getDecl())->printQualifiedName(OS);
    } else {
      OS << (I == Node->varlist_begin() ? StartSym : ',');
      (*I)->printPretty(OS, nullptr, Policy, 0);
    }
  }
}

void OMPClausePrinter::VisitOMPPrivateClause(OMPPrivateClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "private";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPFirstprivateClause(OMPFirstprivateClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "firstprivate";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPLastprivateClause(OMPLastprivateClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "lastprivate";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPSharedClause(OMPSharedClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "shared";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPReductionClause(OMPReductionClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "reduction(";
    NestedNameSpecifier *QualifierLoc =
        Node->getQualifierLoc().getNestedNameSpecifier();
    OverloadedOperatorKind OOK =
        Node->getNameInfo().getName().getCXXOverloadedOperator();
    if (QualifierLoc == nullptr && OOK != OO_None) {
      // Print reduction identifier in C format
      OS << getOperatorSpelling(OOK);
    } else {
      // Use C++ format
      if (QualifierLoc != nullptr)
        QualifierLoc->print(OS, Policy);
      OS << Node->getNameInfo();
    }
    OS << ":";
    VisitOMPClauseList(Node, ' ');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPLinearClause(OMPLinearClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "linear";
    VisitOMPClauseList(Node, '(');
    if (Node->getStep() != nullptr) {
      OS << ": ";
      Node->getStep()->printPretty(OS, nullptr, Policy, 0);
    }
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPAlignedClause(OMPAlignedClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "aligned";
    VisitOMPClauseList(Node, '(');
    if (Node->getAlignment() != nullptr) {
      OS << ": ";
      Node->getAlignment()->printPretty(OS, nullptr, Policy, 0);
    }
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPCopyinClause(OMPCopyinClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "copyin";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPCopyprivateClause(OMPCopyprivateClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "copyprivate";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPFlushClause(OMPFlushClause *Node) {
  if (!Node->varlist_empty()) {
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPDependClause(OMPDependClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "depend(";
    OS << getOpenMPSimpleClauseTypeName(Node->getClauseKind(),
                                        Node->getDependencyKind())
       << " :";
    VisitOMPClauseList(Node, ' ');
    OS << ")";
  }
}
}

//===----------------------------------------------------------------------===//
//  OpenMP directives printing methods
//===----------------------------------------------------------------------===//

void StmtDslPrinter::PrintOMPExecutableDirective(OMPExecutableDirective *S) {
  OMPClausePrinter Printer(OS, Policy);
  ArrayRef<OMPClause *> Clauses = S->clauses();
  for (ArrayRef<OMPClause *>::iterator I = Clauses.begin(), E = Clauses.end();
       I != E; ++I)
    if (*I && !(*I)->isImplicit()) {
      Printer.Visit(*I);
      OS << ' ';
    }
  OS << "\n";
  if (S->hasAssociatedStmt() && S->getAssociatedStmt()) {
    assert(isa<CapturedStmt>(S->getAssociatedStmt()) &&
           "Expected captured statement!");
    Stmt *CS = cast<CapturedStmt>(S->getAssociatedStmt())->getCapturedStmt();
    PrintStmt(CS);
  }
}

void StmtDslPrinter::VisitOMPParallelDirective(OMPParallelDirective *Node) {
  Indent() << "#pragma omp parallel ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPSimdDirective(OMPSimdDirective *Node) {
  Indent() << "#pragma omp simd ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPForDirective(OMPForDirective *Node) {
  Indent() << "#pragma omp for ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPForSimdDirective(OMPForSimdDirective *Node) {
  Indent() << "#pragma omp for simd ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPSectionsDirective(OMPSectionsDirective *Node) {
  Indent() << "#pragma omp sections ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPSectionDirective(OMPSectionDirective *Node) {
  Indent() << "#pragma omp section";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPSingleDirective(OMPSingleDirective *Node) {
  Indent() << "#pragma omp single ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPMasterDirective(OMPMasterDirective *Node) {
  Indent() << "#pragma omp master";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPCriticalDirective(OMPCriticalDirective *Node) {
  Indent() << "#pragma omp critical";
  if (Node->getDirectiveName().getName()) {
    OS << " (";
    Node->getDirectiveName().printName(OS);
    OS << ")";
  }
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPParallelForDirective(OMPParallelForDirective *Node) {
  Indent() << "#pragma omp parallel for ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPParallelForSimdDirective(
    OMPParallelForSimdDirective *Node) {
  Indent() << "#pragma omp parallel for simd ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPParallelSectionsDirective(
    OMPParallelSectionsDirective *Node) {
  Indent() << "#pragma omp parallel sections ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPTaskDirective(OMPTaskDirective *Node) {
  Indent() << "#pragma omp task ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPTaskyieldDirective(OMPTaskyieldDirective *Node) {
  Indent() << "#pragma omp taskyield";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPBarrierDirective(OMPBarrierDirective *Node) {
  Indent() << "#pragma omp barrier";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPTaskwaitDirective(OMPTaskwaitDirective *Node) {
  Indent() << "#pragma omp taskwait";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPTaskgroupDirective(OMPTaskgroupDirective *Node) {
  Indent() << "#pragma omp taskgroup";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPFlushDirective(OMPFlushDirective *Node) {
  Indent() << "#pragma omp flush ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPOrderedDirective(OMPOrderedDirective *Node) {
  Indent() << "#pragma omp ordered";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPAtomicDirective(OMPAtomicDirective *Node) {
  Indent() << "#pragma omp atomic ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPTargetDirective(OMPTargetDirective *Node) {
  Indent() << "#pragma omp target ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPTeamsDirective(OMPTeamsDirective *Node) {
  Indent() << "#pragma omp teams ";
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPCancellationPointDirective(
    OMPCancellationPointDirective *Node) {
  Indent() << "#pragma omp cancellation point "
           << getOpenMPDirectiveName(Node->getCancelRegion());
  PrintOMPExecutableDirective(Node);
}

void StmtDslPrinter::VisitOMPCancelDirective(OMPCancelDirective *Node) {
  Indent() << "#pragma omp cancel "
           << getOpenMPDirectiveName(Node->getCancelRegion());
  PrintOMPExecutableDirective(Node);
}
//===----------------------------------------------------------------------===//
//  Expr printing methods.
//===----------------------------------------------------------------------===//

void StmtDslPrinter::VisitDeclRefExpr(DeclRefExpr *Node) {
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
        OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

void StmtDslPrinter::VisitDependentScopeDeclRefExpr(
                                           DependentScopeDeclRefExpr *Node) {
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
        OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

void StmtDslPrinter::VisitUnresolvedLookupExpr(UnresolvedLookupExpr *Node) {
  if (Node->getQualifier())
    Node->getQualifier()->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
        OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

void StmtDslPrinter::VisitObjCIvarRefExpr(ObjCIvarRefExpr *Node) {
  if (Node->getBase()) {
    PrintExpr(Node->getBase());
    OS << (Node->isArrow() ? "->" : ".");
  }
  OS << *Node->getDecl();
}

void StmtDslPrinter::VisitObjCPropertyRefExpr(ObjCPropertyRefExpr *Node) {
  if (Node->isSuperReceiver())
    OS << "super.";
  else if (Node->isObjectReceiver() && Node->getBase()) {
    PrintExpr(Node->getBase());
    OS << ".";
  } else if (Node->isClassReceiver() && Node->getClassReceiver()) {
    OS << Node->getClassReceiver()->getName() << ".";
  }

  if (Node->isImplicitProperty())
    Node->getImplicitPropertyGetter()->getSelector().print(OS);
  else
    OS << Node->getExplicitProperty()->getName();
}

void StmtDslPrinter::VisitObjCSubscriptRefExpr(ObjCSubscriptRefExpr *Node) {
  
  PrintExpr(Node->getBaseExpr());
  OS << "[";
  PrintExpr(Node->getKeyExpr());
  OS << "]";
}

void StmtDslPrinter::VisitPredefinedExpr(PredefinedExpr *Node) {
  OS << PredefinedExpr::getIdentTypeName(Node->getIdentType());
}

void StmtDslPrinter::VisitCharacterLiteral(CharacterLiteral *Node) {
  unsigned value = Node->getValue();

  switch (Node->getKind()) {
  case CharacterLiteral::Ascii: break; // no prefix.
  case CharacterLiteral::Wide:  OS << 'L'; break;
  case CharacterLiteral::UTF16: OS << 'u'; break;
  case CharacterLiteral::UTF32: OS << 'U'; break;
  }

  switch (value) {
  case '\\':
    OS << "'\\\\'";
    break;
  case '\'':
    OS << "'\\''";
    break;
  case '\a':
    // TODO: K&R: the meaning of '\\a' is different in traditional C
    OS << "'\\a'";
    break;
  case '\b':
    OS << "'\\b'";
    break;
  // Nonstandard escape sequence.
  /*case '\e':
    OS << "'\\e'";
    break;*/
  case '\f':
    OS << "'\\f'";
    break;
  case '\n':
    OS << "'\\n'";
    break;
  case '\r':
    OS << "'\\r'";
    break;
  case '\t':
    OS << "'\\t'";
    break;
  case '\v':
    OS << "'\\v'";
    break;
  default:
    // HLSL Change Begin
    if (Policy.LangOpts.HLSL && value > 255) {
      unsigned int truncVal = value & (~0xffffff00);
      OS << "'\\x" << llvm::format("%02x", truncVal) << "'";
    }
    else {
    // HLSL Change End
    if (value < 256 && isPrintable((unsigned char)value))
      OS << "'" << (char)value << "'";
    else if (value < 256)
      OS << "'\\x" << llvm::format("%02x", value) << "'";
    else if (value <= 0xFFFF)
      OS << "'\\u" << llvm::format("%04x", value) << "'";
    else
      OS << "'\\U" << llvm::format("%08x", value) << "'";
    }
  }
}

void StmtDslPrinter::VisitIntegerLiteral(IntegerLiteral *Node) {
  bool isSigned = Node->getType()->isSignedIntegerType();
  OS << Node->getValue().toString(10, isSigned);

  // Emit suffixes.  Integer literals are always a builtin integer type.
  switch (Node->getType()->getAs<BuiltinType>()->getKind()) {
  default: llvm_unreachable("Unexpected type for integer literal!");
  case BuiltinType::Char_S:
  case BuiltinType::Char_U:    OS << "i8"; break;
  case BuiltinType::UChar:     OS << "Ui8"; break;
  case BuiltinType::Min16Int: // HLSL Change
  case BuiltinType::Short:     OS << "i16"; break;
  case BuiltinType::Min16UInt: // HLSL Change
  case BuiltinType::UShort:    OS << "Ui16"; break;
  case BuiltinType::LitInt:    break; // HLSL Change
  case BuiltinType::Int:       break; // no suffix.
  case BuiltinType::UInt:      OS << 'U'; break;
  case BuiltinType::Long:      OS << 'L'; break;
  case BuiltinType::ULong:     OS << "UL"; break;
  case BuiltinType::LongLong:  OS << "LL"; break;
  case BuiltinType::ULongLong: OS << "ULL"; break;
  case BuiltinType::Int128:    OS << "i128"; break;
  case BuiltinType::UInt128:   OS << "Ui128"; break;
  case BuiltinType::Int8_4Packed:  OS << "i8_4pk"; break; // HLSL Change
  case BuiltinType::UInt8_4Packed: OS << "Ui8_4pk"; break; // HLSL Change
  }
}

static void PrintFloatingLiteral(raw_ostream &OS, FloatingLiteral *Node,
                                 bool PrintSuffix,
                                 bool HLSLSuffix = false) {    // HLSL Change - added HLSLSuffix
  SmallString<16> Str;
  Node->getValue().toString(Str);
  OS << Str;
  if (Str.find_first_not_of("-0123456789") == StringRef::npos)
    OS << '.'; // Trailing dot in order to separate from ints.

  if (!PrintSuffix)
    return;

  // Emit suffixes.  Float literals are always a builtin float type.
  // HLSL Change Starts - override suffixes
  if (HLSLSuffix) {
    switch (Node->getType()->getAs<BuiltinType>()->getKind()) {
    default: llvm_unreachable("Unexpected type for float literal!");
    case BuiltinType::LitFloat:   break; // HLSL Change -- no suffix
    case BuiltinType::Min10Float: break; // no suffix, as this is a literal and 'F' would pollute expression
    case BuiltinType::HalfFloat:
    case BuiltinType::Min16Float:
    case BuiltinType::Half:       OS << 'H'; break; // HLSL Change -- added suffix
    case BuiltinType::Double:     OS << 'L'; break; // HLSL Change -- added suffix
    case BuiltinType::Float:      OS << 'F'; break;
    // Note absence of LongDouble - should not be possible in HLSL
    }
    return;
  }
  // HLSL Change Ends

  // Emit suffixes.  Float literals are always a builtin float type.
  switch (Node->getType()->getAs<BuiltinType>()->getKind()) {
  default: llvm_unreachable("Unexpected type for float literal!");
  case BuiltinType::Half:       break; // FIXME: suffix?
  case BuiltinType::Double:     break; // no suffix.
  case BuiltinType::Float:      OS << 'F'; break;
  case BuiltinType::LongDouble: OS << 'L'; break;
  }
}

void StmtDslPrinter::VisitFloatingLiteral(FloatingLiteral *Node) {
  PrintFloatingLiteral(OS, Node, /*PrintSuffix=*/true, Policy.LangOpts.HLSL);   // HLSL Change
}

void StmtDslPrinter::VisitImaginaryLiteral(ImaginaryLiteral *Node) {
  PrintExpr(Node->getSubExpr());
  OS << "i";
}

void StmtDslPrinter::VisitStringLiteral(StringLiteral *Str) {
  Str->outputString(OS);
}
void StmtDslPrinter::VisitParenExpr(ParenExpr *Node) {
  OS << "(";
  PrintExpr(Node->getSubExpr());
  OS << ")";
}
void StmtDslPrinter::VisitUnaryOperator(UnaryOperator *Node) {
  if (!Node->isPostfix()) {
    OS << UnaryOperator::getOpcodeStr(Node->getOpcode());

    // Print a space if this is an "identifier operator" like __real, or if
    // it might be concatenated incorrectly like '+'.
    switch (Node->getOpcode()) {
    default: break;
    case UO_Real:
    case UO_Imag:
    case UO_Extension:
      OS << ' ';
      break;
    case UO_Plus:
    case UO_Minus:
      if (isa<UnaryOperator>(Node->getSubExpr()))
        OS << ' ';
      break;
    }
  }
  PrintExpr(Node->getSubExpr());

  if (Node->isPostfix())
    OS << UnaryOperator::getOpcodeStr(Node->getOpcode());
}

void StmtDslPrinter::VisitOffsetOfExpr(OffsetOfExpr *Node) {
  OS << "__builtin_offsetof(";
  Node->getTypeSourceInfo()->getType().print(OS, Policy);
  OS << ", ";
  bool PrintedSomething = false;
  for (unsigned i = 0, n = Node->getNumComponents(); i < n; ++i) {
    OffsetOfExpr::OffsetOfNode ON = Node->getComponent(i);
    if (ON.getKind() == OffsetOfExpr::OffsetOfNode::Array) {
      // Array node
      OS << "[";
      PrintExpr(Node->getIndexExpr(ON.getArrayExprIndex()));
      OS << "]";
      PrintedSomething = true;
      continue;
    }

    // Skip implicit base indirections.
    if (ON.getKind() == OffsetOfExpr::OffsetOfNode::Base)
      continue;

    // Field or identifier node.
    IdentifierInfo *Id = ON.getFieldName();
    if (!Id)
      continue;
    
    if (PrintedSomething)
      OS << ".";
    else
      PrintedSomething = true;
    OS << Id->getName();    
  }
  OS << ")";
}

void StmtDslPrinter::VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *Node){
  // HLSL Change Begin
  if (Node->getKind() == UETT_ArrayLength) {
    PrintExpr(Node->getArgumentExpr());
    OS << ".Length";
    return;
  }
  // HLSL Change Ends
  switch(Node->getKind()) {
  case UETT_SizeOf:
    OS << "sizeof";
    break;
  case UETT_AlignOf:
    if (Policy.LangOpts.CPlusPlus)
      OS << "alignof";
    else if (Policy.LangOpts.C11)
      OS << "_Alignof";
    else
      OS << "__alignof";
    break;
  case UETT_VecStep:
    OS << "vec_step";
    break;
  case UETT_OpenMPRequiredSimdAlign:
    OS << "__builtin_omp_required_simd_align";
    break;
  }
  if (Node->isArgumentType()) {
    OS << '(';
    Node->getArgumentType().print(OS, Policy);
    OS << ')';
  } else {
    OS << " ";
    PrintExpr(Node->getArgumentExpr());
  }
}

void StmtDslPrinter::VisitGenericSelectionExpr(GenericSelectionExpr *Node) {
  OS << "_Generic(";
  PrintExpr(Node->getControllingExpr());
  for (unsigned i = 0; i != Node->getNumAssocs(); ++i) {
    OS << ", ";
    QualType T = Node->getAssocType(i);
    if (T.isNull())
      OS << "default";
    else
      T.print(OS, Policy);
    OS << ": ";
    PrintExpr(Node->getAssocExpr(i));
  }
  OS << ")";
}

void StmtDslPrinter::VisitArraySubscriptExpr(ArraySubscriptExpr *Node) {
  PrintExpr(Node->getLHS());
  OS << "[";
  PrintExpr(Node->getRHS());
  OS << "]";
}

void StmtDslPrinter::PrintCallArgs(CallExpr *Call) {
  for (unsigned i = 0, e = Call->getNumArgs(); i != e; ++i) {
    if (isa<CXXDefaultArgExpr>(Call->getArg(i))) {
      // Don't print any defaulted arguments
      break;
    }

    if (i) OS << ", ";
    PrintExpr(Call->getArg(i));
  }
}

void StmtDslPrinter::VisitCallExpr(CallExpr *Call) {
  PrintExpr(Call->getCallee());
  OS << "(";
  PrintCallArgs(Call);
  OS << ")";
}
void StmtDslPrinter::VisitMemberExpr(MemberExpr *Node) {
  // FIXME: Suppress printing implicit bases (like "this")
  PrintExpr(Node->getBase());

  MemberExpr *ParentMember = dyn_cast<MemberExpr>(Node->getBase());
  FieldDecl  *ParentDecl   = ParentMember
    ? dyn_cast<FieldDecl>(ParentMember->getMemberDecl()) : nullptr;

  if (!ParentDecl || !ParentDecl->isAnonymousStructOrUnion())
    OS << (!Policy.LangOpts.HLSL && Node->isArrow() ? "->" : "."); // HLSL Change

  if (FieldDecl *FD = dyn_cast<FieldDecl>(Node->getMemberDecl()))
    if (FD->isAnonymousStructOrUnion())
      return;

  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getMemberNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
        OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}
void StmtDslPrinter::VisitObjCIsaExpr(ObjCIsaExpr *Node) {
  PrintExpr(Node->getBase());
  OS << (Node->isArrow() ? "->isa" : ".isa");
}

void StmtDslPrinter::VisitExtVectorElementExpr(ExtVectorElementExpr *Node) {
  PrintExpr(Node->getBase());
  OS << ".";
  OS << Node->getAccessor().getName();
}
// HLSL Change Starts
void StmtDslPrinter::VisitExtMatrixElementExpr(ExtMatrixElementExpr *Node) {
  PrintExpr(Node->getBase());
  OS << ".";
  OS << Node->getAccessor().getName();
}
void StmtDslPrinter::VisitHLSLVectorElementExpr(HLSLVectorElementExpr *Node) {
  PrintExpr(Node->getBase());
  OS << ".";
  OS << Node->getAccessor().getName();
}
// HLSL Change Ends
void StmtDslPrinter::VisitCStyleCastExpr(CStyleCastExpr *Node) {
  OS << '(';
  Node->getTypeAsWritten().print(OS, Policy);
  OS << ')';
  PrintExpr(Node->getSubExpr());
}
void StmtDslPrinter::VisitCompoundLiteralExpr(CompoundLiteralExpr *Node) {
  OS << '(';
  Node->getType().print(OS, Policy);
  OS << ')';
  PrintExpr(Node->getInitializer());
}
void StmtDslPrinter::VisitImplicitCastExpr(ImplicitCastExpr *Node) {
  // No need to print anything, simply forward to the subexpression.
  PrintExpr(Node->getSubExpr());
}
void StmtDslPrinter::VisitBinaryOperator(BinaryOperator *Node) {
  PrintExpr(Node->getLHS());
  OS << " " << BinaryOperator::getOpcodeStr(Node->getOpcode()) << " ";
  PrintExpr(Node->getRHS());
}
void StmtDslPrinter::VisitCompoundAssignOperator(CompoundAssignOperator *Node) {
  PrintExpr(Node->getLHS());
  OS << " " << BinaryOperator::getOpcodeStr(Node->getOpcode()) << " ";
  PrintExpr(Node->getRHS());
}
void StmtDslPrinter::VisitConditionalOperator(ConditionalOperator *Node) {
  PrintExpr(Node->getCond());
  OS << " ? ";
  PrintExpr(Node->getLHS());
  OS << " : ";
  PrintExpr(Node->getRHS());
}

// GNU extensions.

void
StmtDslPrinter::VisitBinaryConditionalOperator(BinaryConditionalOperator *Node) {
  PrintExpr(Node->getCommon());
  OS << " ?: ";
  PrintExpr(Node->getFalseExpr());
}
void StmtDslPrinter::VisitAddrLabelExpr(AddrLabelExpr *Node) {
  OS << "&&" << Node->getLabel()->getName();
}

void StmtDslPrinter::VisitStmtExpr(StmtExpr *E) {
  OS << "(";
  PrintRawCompoundStmt(E->getSubStmt());
  OS << ")";
}

void StmtDslPrinter::VisitChooseExpr(ChooseExpr *Node) {
  OS << "__builtin_choose_expr(";
  PrintExpr(Node->getCond());
  OS << ", ";
  PrintExpr(Node->getLHS());
  OS << ", ";
  PrintExpr(Node->getRHS());
  OS << ")";
}

void StmtDslPrinter::VisitGNUNullExpr(GNUNullExpr *) {
  OS << "__null";
}

void StmtDslPrinter::VisitShuffleVectorExpr(ShuffleVectorExpr *Node) {
  OS << "__builtin_shufflevector(";
  for (unsigned i = 0, e = Node->getNumSubExprs(); i != e; ++i) {
    if (i) OS << ", ";
    PrintExpr(Node->getExpr(i));
  }
  OS << ")";
}

void StmtDslPrinter::VisitConvertVectorExpr(ConvertVectorExpr *Node) {
  OS << "__builtin_convertvector(";
  PrintExpr(Node->getSrcExpr());
  OS << ", ";
  Node->getType().print(OS, Policy);
  OS << ")";
}

void StmtDslPrinter::VisitInitListExpr(InitListExpr* Node) {
  if (Node->getSyntacticForm()) {
    Visit(Node->getSyntacticForm());
    return;
  }

  if (!Policy.LangOpts.HLSL ||
      !Node->isVectorInitWithCXXFunctionalCastExpr()) // HLSL Change
    OS << "{ "; 

  for (unsigned i = 0, e = Node->getNumInits(); i != e; ++i) {
    if (i) OS << ", ";
    if (Node->getInit(i))
      PrintExpr(Node->getInit(i));
    else
      OS << "{}";
  }

  if (!Policy.LangOpts.HLSL ||
      !Node->isVectorInitWithCXXFunctionalCastExpr()) // HLSL Change
    OS << " }";
}

void StmtDslPrinter::VisitParenListExpr(ParenListExpr* Node) {
  OS << "(";
  for (unsigned i = 0, e = Node->getNumExprs(); i != e; ++i) {
    if (i) OS << ", ";
    PrintExpr(Node->getExpr(i));
  }
  OS << ")";
}

void StmtDslPrinter::VisitDesignatedInitExpr(DesignatedInitExpr *Node) {
  bool NeedsEquals = true;
  for (DesignatedInitExpr::designators_iterator D = Node->designators_begin(),
                      DEnd = Node->designators_end();
       D != DEnd; ++D) {
    if (D->isFieldDesignator()) {
      if (D->getDotLoc().isInvalid()) {
        if (IdentifierInfo *II = D->getFieldName()) {
          OS << II->getName() << ":";
          NeedsEquals = false;
        }
      } else {
        OS << "." << D->getFieldName()->getName();
      }
    } else {
      OS << "[";
      if (D->isArrayDesignator()) {
        PrintExpr(Node->getArrayIndex(*D));
      } else {
        PrintExpr(Node->getArrayRangeStart(*D));
        OS << " ... ";
        PrintExpr(Node->getArrayRangeEnd(*D));
      }
      OS << "]";
    }
  }

  if (NeedsEquals)
    OS << " = ";
  else
    OS << " ";
  PrintExpr(Node->getInit());
}

void StmtDslPrinter::VisitDesignatedInitUpdateExpr(
    DesignatedInitUpdateExpr *Node) {
  OS << "{";
  OS << "/*base*/";
  PrintExpr(Node->getBase());
  OS << ", ";

  OS << "/*updater*/";
  PrintExpr(Node->getUpdater());
  OS << "}";
}

void StmtDslPrinter::VisitNoInitExpr(NoInitExpr *Node) {
  OS << "/*no init*/";
}

void StmtDslPrinter::VisitImplicitValueInitExpr(ImplicitValueInitExpr *Node) {
  if (Policy.LangOpts.CPlusPlus) {
    OS << "/*implicit*/";
    Node->getType().print(OS, Policy);
    OS << "()";
  } else {
    OS << "/*implicit*/(";
    Node->getType().print(OS, Policy);
    OS << ')';
    if (Node->getType()->isRecordType())
      OS << "{}";
    else
      OS << 0;
  }
}

void StmtDslPrinter::VisitVAArgExpr(VAArgExpr *Node) {
  OS << "__builtin_va_arg(";
  PrintExpr(Node->getSubExpr());
  OS << ", ";
  Node->getType().print(OS, Policy);
  OS << ")";
}

void StmtDslPrinter::VisitPseudoObjectExpr(PseudoObjectExpr *Node) {
  PrintExpr(Node->getSyntacticForm());
}

void StmtDslPrinter::VisitAtomicExpr(AtomicExpr *Node) {
  const char *Name = nullptr;
  switch (Node->getOp()) {
#define BUILTIN(ID, TYPE, ATTRS)
#define ATOMIC_BUILTIN(ID, TYPE, ATTRS) \
  case AtomicExpr::AO ## ID: \
    Name = #ID "("; \
    break;
#include "clang/Basic/Builtins.def"
  }
  OS << Name;

  // AtomicExpr stores its subexpressions in a permuted order.
  PrintExpr(Node->getPtr());
  if (Node->getOp() != AtomicExpr::AO__c11_atomic_load &&
      Node->getOp() != AtomicExpr::AO__atomic_load_n) {
    OS << ", ";
    PrintExpr(Node->getVal1());
  }
  if (Node->getOp() == AtomicExpr::AO__atomic_exchange ||
      Node->isCmpXChg()) {
    OS << ", ";
    PrintExpr(Node->getVal2());
  }
  if (Node->getOp() == AtomicExpr::AO__atomic_compare_exchange ||
      Node->getOp() == AtomicExpr::AO__atomic_compare_exchange_n) {
    OS << ", ";
    PrintExpr(Node->getWeak());
  }
  if (Node->getOp() != AtomicExpr::AO__c11_atomic_init) {
    OS << ", ";
    PrintExpr(Node->getOrder());
  }
  if (Node->isCmpXChg()) {
    OS << ", ";
    PrintExpr(Node->getOrderFail());
  }
  OS << ")";
}

// C++
void StmtDslPrinter::VisitCXXOperatorCallExpr(CXXOperatorCallExpr *Node) {
  const char *OpStrings[NUM_OVERLOADED_OPERATORS] = {
    "",
#define OVERLOADED_OPERATOR(Name,Spelling,Token,Unary,Binary,MemberOnly) \
    Spelling,
#include "clang/Basic/OperatorKinds.def"
  };

  OverloadedOperatorKind Kind = Node->getOperator();
  if (Kind == OO_PlusPlus || Kind == OO_MinusMinus) {
    if (Node->getNumArgs() == 1) {
      OS << OpStrings[Kind] << ' ';
      PrintExpr(Node->getArg(0));
    } else {
      PrintExpr(Node->getArg(0));
      OS << ' ' << OpStrings[Kind];
    }
  } else if (Kind == OO_Arrow) {
    PrintExpr(Node->getArg(0));
  } else if (Kind == OO_Call) {
    PrintExpr(Node->getArg(0));
    OS << '(';
    for (unsigned ArgIdx = 1; ArgIdx < Node->getNumArgs(); ++ArgIdx) {
      if (ArgIdx > 1)
        OS << ", ";
      if (!isa<CXXDefaultArgExpr>(Node->getArg(ArgIdx)))
        PrintExpr(Node->getArg(ArgIdx));
    }
    OS << ')';
  } else if (Kind == OO_Subscript) {
    PrintExpr(Node->getArg(0));
    OS << '[';
    PrintExpr(Node->getArg(1));
    OS << ']';
  } else if (Node->getNumArgs() == 1) {
    OS << OpStrings[Kind] << ' ';
    PrintExpr(Node->getArg(0));
  } else if (Node->getNumArgs() == 2) {
    PrintExpr(Node->getArg(0));
    OS << ' ' << OpStrings[Kind] << ' ';
    PrintExpr(Node->getArg(1));
  } else {
    llvm_unreachable("unknown overloaded operator");
  }
}

void StmtDslPrinter::VisitCXXMemberCallExpr(CXXMemberCallExpr *Node) {
  // If we have a conversion operator call only print the argument.
  CXXMethodDecl *MD = Node->getMethodDecl();
  if (MD && isa<CXXConversionDecl>(MD)) {
    PrintExpr(Node->getImplicitObjectArgument());
    return;
  }
  VisitCallExpr(cast<CallExpr>(Node));
}

void StmtDslPrinter::VisitCUDAKernelCallExpr(CUDAKernelCallExpr *Node) {
  PrintExpr(Node->getCallee());
  OS << "<<<";
  PrintCallArgs(Node->getConfig());
  OS << ">>>(";
  PrintCallArgs(Node);
  OS << ")";
}

void StmtDslPrinter::VisitCXXNamedCastExpr(CXXNamedCastExpr *Node) {
  OS << Node->getCastName() << '<';
  Node->getTypeAsWritten().print(OS, Policy);
  OS << ">(";
  PrintExpr(Node->getSubExpr());
  OS << ")";
}

void StmtDslPrinter::VisitCXXStaticCastExpr(CXXStaticCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void StmtDslPrinter::VisitCXXDynamicCastExpr(CXXDynamicCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void StmtDslPrinter::VisitCXXReinterpretCastExpr(CXXReinterpretCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void StmtDslPrinter::VisitCXXConstCastExpr(CXXConstCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void StmtDslPrinter::VisitCXXTypeidExpr(CXXTypeidExpr *Node) {
  OS << "typeid(";
  if (Node->isTypeOperand()) {
    Node->getTypeOperandSourceInfo()->getType().print(OS, Policy);
  } else {
    PrintExpr(Node->getExprOperand());
  }
  OS << ")";
}

void StmtDslPrinter::VisitCXXUuidofExpr(CXXUuidofExpr *Node) {
  OS << "__uuidof(";
  if (Node->isTypeOperand()) {
    Node->getTypeOperandSourceInfo()->getType().print(OS, Policy);
  } else {
    PrintExpr(Node->getExprOperand());
  }
  OS << ")";
}

void StmtDslPrinter::VisitMSPropertyRefExpr(MSPropertyRefExpr *Node) {
  PrintExpr(Node->getBaseExpr());
  if (Node->isArrow())
    OS << "->";
  else
    OS << ".";
  if (NestedNameSpecifier *Qualifier =
      Node->getQualifierLoc().getNestedNameSpecifier())
    Qualifier->print(OS, Policy);
  OS << Node->getPropertyDecl()->getDeclName();
}

void StmtDslPrinter::VisitUserDefinedLiteral(UserDefinedLiteral *Node) {
  switch (Node->getLiteralOperatorKind()) {
  case UserDefinedLiteral::LOK_Raw:
    OS << cast<StringLiteral>(Node->getArg(0)->IgnoreImpCasts())->getString();
    break;
  case UserDefinedLiteral::LOK_Template: {
    DeclRefExpr *DRE = cast<DeclRefExpr>(Node->getCallee()->IgnoreImpCasts());
    const TemplateArgumentList *Args =
      cast<FunctionDecl>(DRE->getDecl())->getTemplateSpecializationArgs();
    assert(Args);

    if (Args->size() != 1) {
      OS << "operator \"\" " << Node->getUDSuffix()->getName();
      TemplateSpecializationType::PrintTemplateArgumentList(
          OS, Args->data(), Args->size(), Policy);
      OS << "()";
      return;
    }

    const TemplateArgument &Pack = Args->get(0);
    for (const auto &P : Pack.pack_elements()) {
      char C = (char)P.getAsIntegral().getZExtValue();
      OS << C;
    }
    break;
  }
  case UserDefinedLiteral::LOK_Integer: {
    // Print integer literal without suffix.
    IntegerLiteral *Int = cast<IntegerLiteral>(Node->getCookedLiteral());
    OS << Int->getValue().toString(10, /*isSigned*/false);
    break;
  }
  case UserDefinedLiteral::LOK_Floating: {
    // Print floating literal without suffix.
    FloatingLiteral *Float = cast<FloatingLiteral>(Node->getCookedLiteral());
    PrintFloatingLiteral(OS, Float, /*PrintSuffix=*/false, Policy.LangOpts.HLSL);   // HLSL Change
    break;
  }
  case UserDefinedLiteral::LOK_String:
  case UserDefinedLiteral::LOK_Character:
    PrintExpr(Node->getCookedLiteral());
    break;
  }
  OS << Node->getUDSuffix()->getName();
}

void StmtDslPrinter::VisitCXXBoolLiteralExpr(CXXBoolLiteralExpr *Node) {
  OS << (Node->getValue() ? "true" : "false");
}

void StmtDslPrinter::VisitCXXNullPtrLiteralExpr(CXXNullPtrLiteralExpr *Node) {
  OS << "nullptr";
}

void StmtDslPrinter::VisitCXXThisExpr(CXXThisExpr *Node) {
  OS << "this";
}

void StmtDslPrinter::VisitCXXThrowExpr(CXXThrowExpr *Node) {
  if (!Node->getSubExpr())
    OS << "throw";
  else {
    OS << "throw ";
    PrintExpr(Node->getSubExpr());
  }
}

void StmtDslPrinter::VisitCXXDefaultArgExpr(CXXDefaultArgExpr *Node) {
  // Nothing to print: we picked up the default argument.
}

void StmtDslPrinter::VisitCXXDefaultInitExpr(CXXDefaultInitExpr *Node) {
  // Nothing to print: we picked up the default initializer.
}

void StmtDslPrinter::VisitCXXFunctionalCastExpr(CXXFunctionalCastExpr *Node) {
  Node->getType().print(OS, Policy);
  // If there are no parens, this is list-initialization, and the braces are
  // part of the syntax of the inner construct.
  if (Node->getLParenLoc().isValid())
    OS << "(";
  PrintExpr(Node->getSubExpr());
  if (Node->getLParenLoc().isValid())
    OS << ")";
}

void StmtDslPrinter::VisitCXXBindTemporaryExpr(CXXBindTemporaryExpr *Node) {
  PrintExpr(Node->getSubExpr());
}

void StmtDslPrinter::VisitCXXTemporaryObjectExpr(CXXTemporaryObjectExpr *Node) {
  Node->getType().print(OS, Policy);
  if (Node->isStdInitListInitialization())
    /* Nothing to do; braces are part of creating the std::initializer_list. */;
  else if (Node->isListInitialization())
    OS << "{";
  else
    OS << "(";
  for (CXXTemporaryObjectExpr::arg_iterator Arg = Node->arg_begin(),
                                         ArgEnd = Node->arg_end();
       Arg != ArgEnd; ++Arg) {
    if (Arg->isDefaultArgument())
      break;
    if (Arg != Node->arg_begin())
      OS << ", ";
    PrintExpr(*Arg);
  }
  if (Node->isStdInitListInitialization())
    /* See above. */;
  else if (Node->isListInitialization())
    OS << "}";
  else
    OS << ")";
}

void StmtDslPrinter::VisitLambdaExpr(LambdaExpr *Node) {
  OS << '[';
  bool NeedComma = false;
  switch (Node->getCaptureDefault()) {
  case LCD_None:
    break;

  case LCD_ByCopy:
    OS << '=';
    NeedComma = true;
    break;

  case LCD_ByRef:
    OS << '&';
    NeedComma = true;
    break;
  }
  for (LambdaExpr::capture_iterator C = Node->explicit_capture_begin(),
                                 CEnd = Node->explicit_capture_end();
       C != CEnd;
       ++C) {
    if (NeedComma)
      OS << ", ";
    NeedComma = true;

    switch (C->getCaptureKind()) {
    case LCK_This:
      OS << "this";
      break;

    case LCK_ByRef:
      if (Node->getCaptureDefault() != LCD_ByRef || Node->isInitCapture(C))
        OS << '&';
      OS << C->getCapturedVar()->getName();
      break;

    case LCK_ByCopy:
      OS << C->getCapturedVar()->getName();
      break;
    case LCK_VLAType:
      llvm_unreachable("VLA type in explicit captures.");
    }

    if (Node->isInitCapture(C))
      PrintExpr(C->getCapturedVar()->getInit());
  }
  OS << ']';

  if (Node->hasExplicitParameters()) {
    OS << " (";
    CXXMethodDecl *Method = Node->getCallOperator();
    NeedComma = false;
    for (auto P : Method->params()) {
      if (NeedComma) {
        OS << ", ";
      } else {
        NeedComma = true;
      }
      std::string ParamStr = P->getNameAsString();
      P->getOriginalType().print(OS, Policy, ParamStr);
    }
    if (Method->isVariadic()) {
      if (NeedComma)
        OS << ", ";
      OS << "...";
    }
    OS << ')';

    if (Node->isMutable())
      OS << " mutable";

    const FunctionProtoType *Proto
      = Method->getType()->getAs<FunctionProtoType>();
    Proto->printExceptionSpecification(OS, Policy);

    // FIXME: Attributes

    // Print the trailing return type if it was specified in the source.
    if (Node->hasExplicitResultType()) {
      OS << " -> ";
      Proto->getReturnType().print(OS, Policy);
    }
  }

  // Print the body.
  CompoundStmt *Body = Node->getBody();
  OS << ' ';
  PrintStmt(Body);
}

void StmtDslPrinter::VisitCXXScalarValueInitExpr(CXXScalarValueInitExpr *Node) {
  if (TypeSourceInfo *TSInfo = Node->getTypeSourceInfo())
    TSInfo->getType().print(OS, Policy);
  else
    Node->getType().print(OS, Policy);
  OS << "()";
}

void StmtDslPrinter::VisitCXXNewExpr(CXXNewExpr *E) {
  if (E->isGlobalNew())
    OS << "::";
  OS << "new ";
  unsigned NumPlace = E->getNumPlacementArgs();
  if (NumPlace > 0 && !isa<CXXDefaultArgExpr>(E->getPlacementArg(0))) {
    OS << "(";
    PrintExpr(E->getPlacementArg(0));
    for (unsigned i = 1; i < NumPlace; ++i) {
      if (isa<CXXDefaultArgExpr>(E->getPlacementArg(i)))
        break;
      OS << ", ";
      PrintExpr(E->getPlacementArg(i));
    }
    OS << ") ";
  }
  if (E->isParenTypeId())
    OS << "(";
  std::string TypeS;
  if (Expr *Size = E->getArraySize()) {
    llvm::raw_string_ostream s(TypeS);
    s << '[';
    Size->printPretty(s, Helper, Policy);
    s << ']';
  }
  E->getAllocatedType().print(OS, Policy, TypeS);
  if (E->isParenTypeId())
    OS << ")";

  CXXNewExpr::InitializationStyle InitStyle = E->getInitializationStyle();
  if (InitStyle) {
    if (InitStyle == CXXNewExpr::CallInit)
      OS << "(";
    PrintExpr(E->getInitializer());
    if (InitStyle == CXXNewExpr::CallInit)
      OS << ")";
  }
}

void StmtDslPrinter::VisitCXXDeleteExpr(CXXDeleteExpr *E) {
  if (E->isGlobalDelete())
    OS << "::";
  OS << "delete ";
  if (E->isArrayForm())
    OS << "[] ";
  PrintExpr(E->getArgument());
}

void StmtDslPrinter::VisitCXXPseudoDestructorExpr(CXXPseudoDestructorExpr *E) {
  PrintExpr(E->getBase());
  if (E->isArrow())
    OS << "->";
  else
    OS << '.';
  if (E->getQualifier())
    E->getQualifier()->print(OS, Policy);
  OS << "~";

  if (IdentifierInfo *II = E->getDestroyedTypeIdentifier())
    OS << II->getName();
  else
    E->getDestroyedType().print(OS, Policy);
}

void StmtDslPrinter::VisitCXXConstructExpr(CXXConstructExpr *E) {
  if (E->isListInitialization() && !E->isStdInitListInitialization())
    OS << "{";

  for (unsigned i = 0, e = E->getNumArgs(); i != e; ++i) {
    if (isa<CXXDefaultArgExpr>(E->getArg(i))) {
      // Don't print any defaulted arguments
      break;
    }

    if (i) OS << ", ";
    PrintExpr(E->getArg(i));
  }

  if (E->isListInitialization() && !E->isStdInitListInitialization())
    OS << "}";
}

void StmtDslPrinter::VisitCXXStdInitializerListExpr(CXXStdInitializerListExpr *E) {
  PrintExpr(E->getSubExpr());
}

void StmtDslPrinter::VisitExprWithCleanups(ExprWithCleanups *E) {
  // Just forward to the subexpression.
  PrintExpr(E->getSubExpr());
}

void
StmtDslPrinter::VisitCXXUnresolvedConstructExpr(
                                           CXXUnresolvedConstructExpr *Node) {
  Node->getTypeAsWritten().print(OS, Policy);
  OS << "(";
  for (CXXUnresolvedConstructExpr::arg_iterator Arg = Node->arg_begin(),
                                             ArgEnd = Node->arg_end();
       Arg != ArgEnd; ++Arg) {
    if (Arg != Node->arg_begin())
      OS << ", ";
    PrintExpr(*Arg);
  }
  OS << ")";
}

void StmtDslPrinter::VisitCXXDependentScopeMemberExpr(
                                         CXXDependentScopeMemberExpr *Node) {
  if (!Node->isImplicitAccess()) {
    PrintExpr(Node->getBase());
    OS << (Node->isArrow() ? "->" : ".");
  }
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getMemberNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
        OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

void StmtDslPrinter::VisitUnresolvedMemberExpr(UnresolvedMemberExpr *Node) {
  if (!Node->isImplicitAccess()) {
    PrintExpr(Node->getBase());
    OS << (Node->isArrow() ? "->" : ".");
  }
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getMemberNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
        OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

static const char *getTypeTraitName(TypeTrait TT) {
  switch (TT) {
#define TYPE_TRAIT_1(Spelling, Name, Key) \
case clang::UTT_##Name: return #Spelling;
#define TYPE_TRAIT_2(Spelling, Name, Key) \
case clang::BTT_##Name: return #Spelling;
#define TYPE_TRAIT_N(Spelling, Name, Key) \
  case clang::TT_##Name: return #Spelling;
#include "clang/Basic/TokenKinds.def"
  }
  llvm_unreachable("Type trait not covered by switch");
}

static const char *getTypeTraitName(ArrayTypeTrait ATT) {
  switch (ATT) {
  case ATT_ArrayRank:        return "__array_rank";
  case ATT_ArrayExtent:      return "__array_extent";
  }
  llvm_unreachable("Array type trait not covered by switch");
}

static const char *getExpressionTraitName(ExpressionTrait ET) {
  switch (ET) {
  case ET_IsLValueExpr:      return "__is_lvalue_expr";
  case ET_IsRValueExpr:      return "__is_rvalue_expr";
  }
  llvm_unreachable("Expression type trait not covered by switch");
}

void StmtDslPrinter::VisitTypeTraitExpr(TypeTraitExpr *E) {
  OS << getTypeTraitName(E->getTrait()) << "(";
  for (unsigned I = 0, N = E->getNumArgs(); I != N; ++I) {
    if (I > 0)
      OS << ", ";
    E->getArg(I)->getType().print(OS, Policy);
  }
  OS << ")";
}

void StmtDslPrinter::VisitArrayTypeTraitExpr(ArrayTypeTraitExpr *E) {
  OS << getTypeTraitName(E->getTrait()) << '(';
  E->getQueriedType().print(OS, Policy);
  OS << ')';
}

void StmtDslPrinter::VisitExpressionTraitExpr(ExpressionTraitExpr *E) {
  OS << getExpressionTraitName(E->getTrait()) << '(';
  PrintExpr(E->getQueriedExpression());
  OS << ')';
}

void StmtDslPrinter::VisitCXXNoexceptExpr(CXXNoexceptExpr *E) {
  OS << "noexcept(";
  PrintExpr(E->getOperand());
  OS << ")";
}

void StmtDslPrinter::VisitPackExpansionExpr(PackExpansionExpr *E) {
  PrintExpr(E->getPattern());
  OS << "...";
}

void StmtDslPrinter::VisitSizeOfPackExpr(SizeOfPackExpr *E) {
  OS << "sizeof...(" << *E->getPack() << ")";
}

void StmtDslPrinter::VisitSubstNonTypeTemplateParmPackExpr(
                                       SubstNonTypeTemplateParmPackExpr *Node) {
  OS << *Node->getParameterPack();
}

void StmtDslPrinter::VisitSubstNonTypeTemplateParmExpr(
                                       SubstNonTypeTemplateParmExpr *Node) {
  Visit(Node->getReplacement());
}

void StmtDslPrinter::VisitFunctionParmPackExpr(FunctionParmPackExpr *E) {
  OS << *E->getParameterPack();
}

void StmtDslPrinter::VisitMaterializeTemporaryExpr(MaterializeTemporaryExpr *Node){
  PrintExpr(Node->GetTemporaryExpr());
}

void StmtDslPrinter::VisitCXXFoldExpr(CXXFoldExpr *E) {
  OS << "(";
  if (E->getLHS()) {
    PrintExpr(E->getLHS());
    OS << " " << BinaryOperator::getOpcodeStr(E->getOperator()) << " ";
  }
  OS << "...";
  if (E->getRHS()) {
    OS << " " << BinaryOperator::getOpcodeStr(E->getOperator()) << " ";
    PrintExpr(E->getRHS());
  }
  OS << ")";
}

// Obj-C

void StmtDslPrinter::VisitObjCStringLiteral(ObjCStringLiteral *Node) {
  OS << "@";
  VisitStringLiteral(Node->getString());
}

void StmtDslPrinter::VisitObjCBoxedExpr(ObjCBoxedExpr *E) {
  OS << "@";
  Visit(E->getSubExpr());
}

void StmtDslPrinter::VisitObjCArrayLiteral(ObjCArrayLiteral *E) {
  OS << "@[ ";
  StmtRange ch = E->children();
  if (ch.first != ch.second) {
    while (1) {
      Visit(*ch.first);
      ++ch.first;
      if (ch.first == ch.second) break;
      OS << ", ";
    }
  }
  OS << " ]";
}

void StmtDslPrinter::VisitObjCDictionaryLiteral(ObjCDictionaryLiteral *E) {
  OS << "@{ ";
  for (unsigned I = 0, N = E->getNumElements(); I != N; ++I) {
    if (I > 0)
      OS << ", ";
    
    ObjCDictionaryElement Element = E->getKeyValueElement(I);
    Visit(Element.Key);
    OS << " : ";
    Visit(Element.Value);
    if (Element.isPackExpansion())
      OS << "...";
  }
  OS << " }";
}

void StmtDslPrinter::VisitObjCEncodeExpr(ObjCEncodeExpr *Node) {
  OS << "@encode(";
  Node->getEncodedType().print(OS, Policy);
  OS << ')';
}

void StmtDslPrinter::VisitObjCSelectorExpr(ObjCSelectorExpr *Node) {
  OS << "@selector(";
  Node->getSelector().print(OS);
  OS << ')';
}

void StmtDslPrinter::VisitObjCProtocolExpr(ObjCProtocolExpr *Node) {
  OS << "@protocol(" << *Node->getProtocol() << ')';
}

void StmtDslPrinter::VisitObjCMessageExpr(ObjCMessageExpr *Mess) {
  OS << "[";
  switch (Mess->getReceiverKind()) {
  case ObjCMessageExpr::Instance:
    PrintExpr(Mess->getInstanceReceiver());
    break;

  case ObjCMessageExpr::Class:
    Mess->getClassReceiver().print(OS, Policy);
    break;

  case ObjCMessageExpr::SuperInstance:
  case ObjCMessageExpr::SuperClass:
    OS << "Super";
    break;
  }

  OS << ' ';
  Selector selector = Mess->getSelector();
  if (selector.isUnarySelector()) {
    OS << selector.getNameForSlot(0);
  } else {
    for (unsigned i = 0, e = Mess->getNumArgs(); i != e; ++i) {
      if (i < selector.getNumArgs()) {
        if (i > 0) OS << ' ';
        if (selector.getIdentifierInfoForSlot(i))
          OS << selector.getIdentifierInfoForSlot(i)->getName() << ':';
        else
           OS << ":";
      }
      else OS << ", "; // Handle variadic methods.

      PrintExpr(Mess->getArg(i));
    }
  }
  OS << "]";
}

void StmtDslPrinter::VisitObjCBoolLiteralExpr(ObjCBoolLiteralExpr *Node) {
  OS << (Node->getValue() ? "__objc_yes" : "__objc_no");
}

void
StmtDslPrinter::VisitObjCIndirectCopyRestoreExpr(ObjCIndirectCopyRestoreExpr *E) {
  PrintExpr(E->getSubExpr());
}

void
StmtDslPrinter::VisitObjCBridgedCastExpr(ObjCBridgedCastExpr *E) {
  OS << '(' << E->getBridgeKindName();
  E->getType().print(OS, Policy);
  OS << ')';
  PrintExpr(E->getSubExpr());
}

void StmtDslPrinter::VisitBlockExpr(BlockExpr *Node) {
  BlockDecl *BD = Node->getBlockDecl();
  OS << "^";

  const FunctionType *AFT = Node->getFunctionType();

  if (isa<FunctionNoProtoType>(AFT)) {
    OS << "()";
  } else if (!BD->param_empty() || cast<FunctionProtoType>(AFT)->isVariadic()) {
    OS << '(';
    for (BlockDecl::param_iterator AI = BD->param_begin(),
         E = BD->param_end(); AI != E; ++AI) {
      if (AI != BD->param_begin()) OS << ", ";
      std::string ParamStr = (*AI)->getNameAsString();
      (*AI)->getType().print(OS, Policy, ParamStr);
    }

    const FunctionProtoType *FT = cast<FunctionProtoType>(AFT);
    if (FT->isVariadic()) {
      if (!BD->param_empty()) OS << ", ";
      OS << "...";
    }
    OS << ')';
  }
  OS << "{ }";
}

void StmtDslPrinter::VisitOpaqueValueExpr(OpaqueValueExpr *Node) { 
  PrintExpr(Node->getSourceExpr());
}

void StmtDslPrinter::VisitTypoExpr(TypoExpr *Node) {
  // TODO: Print something reasonable for a TypoExpr, if necessary.
  assert(false && "Cannot print TypoExpr nodes");
}

void StmtDslPrinter::VisitAsTypeExpr(AsTypeExpr *Node) {
  OS << "__builtin_astype(";
  PrintExpr(Node->getSrcExpr());
  OS << ", ";
  Node->getType().print(OS, Policy);
  OS << ")";
}

//===----------------------------------------------------------------------===//
// Stmt method implementations
//===----------------------------------------------------------------------===//

void Stmt::dumpPretty(const ASTContext &Context) const {
  printPretty(llvm::errs(), nullptr, PrintingPolicy(Context.getLangOpts()));
}

void Stmt::printPretty(raw_ostream &OS,
                       PrinterHelper *Helper,
                       const PrintingPolicy &Policy,
                       unsigned Indentation) const {
  StmtDslPrinter P(OS, Helper, Policy, Indentation);
  P.Visit(const_cast<Stmt*>(this));
}

//===----------------------------------------------------------------------===//
// PrinterHelper
//===----------------------------------------------------------------------===//

// Implement virtual destructor.
PrinterHelper::~PrinterHelper() {}
