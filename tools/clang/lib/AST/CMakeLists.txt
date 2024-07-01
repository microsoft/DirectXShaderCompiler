set(LLVM_LINK_COMPONENTS ScalarOpts support)

# Merged AST and Analysis into one, since they have a circular dependency, no bueno.

set (HLSL_IGNORE_SOURCES
  NSAPI.cpp
  ../Analysis/CocoaConventions.cpp
  ../Analysis/FormatString.cpp
  ../Analysis/PrintfFormatString.cpp
  ../Analysis/ScanfFormatString.cpp
  )

add_clang_library(clangAST

  # AST

  APValue.cpp
  ASTConsumer.cpp
  ASTContext.cpp
  ASTContextHLSL.cpp # HLSL Change
  ASTDiagnostic.cpp
  ASTDumper.cpp
  ASTImporter.cpp
  ASTTypeTraits.cpp
  AttrImpl.cpp
  CXXInheritance.cpp
  Comment.cpp
  CommentBriefParser.cpp
  CommentCommandTraits.cpp
  CommentLexer.cpp
  CommentParser.cpp
  CommentSema.cpp
  Decl.cpp
  DeclarationName.cpp
  DeclBase.cpp
  DeclCXX.cpp
  DeclFriend.cpp
  DeclGroup.cpp
  DeclObjC.cpp
  DeclOpenMP.cpp
  DeclPrinter.cpp
  DeclTemplate.cpp
  Expr.cpp
  ExprClassification.cpp
  ExprConstant.cpp
  ExprCXX.cpp
  ExternalASTSource.cpp
  HlslBuiltinTypeDeclBuilder.cpp # HLSL Change
  HlslTypes.cpp # HLSL Change
  InheritViz.cpp
  ItaniumCXXABI.cpp
  ItaniumMangle.cpp
  Mangle.cpp
  MicrosoftCXXABI.cpp
  MicrosoftMangle.cpp
  NestedNameSpecifier.cpp
  ParentMap.cpp
  RawCommentList.cpp
  RecordLayout.cpp
  RecordLayoutBuilder.cpp
  SelectorLocationsKind.cpp
  Stmt.cpp
  StmtIterator.cpp
  StmtPrinter.cpp
  StmtProfile.cpp
  StmtViz.cpp
  TemplateBase.cpp
  TemplateName.cpp
  Type.cpp
  TypeLoc.cpp
  TypePrinter.cpp
  VTableBuilder.cpp
  VTTBuilder.cpp

  # Analysis

  ../Analysis/AnalysisDeclContext.cpp
  ../Analysis/BodyFarm.cpp
  ../Analysis/CFG.cpp
  ../Analysis/CFGReachabilityAnalysis.cpp
  ../Analysis/CFGStmtMap.cpp
  ../Analysis/CallGraph.cpp
  ../Analysis/Consumed.cpp
  ../Analysis/CodeInjector.cpp
  ../Analysis/Dominators.cpp
  ../Analysis/LiveVariables.cpp
  ../Analysis/ObjCNoReturn.cpp
  ../Analysis/PostOrderCFGView.cpp
  ../Analysis/ProgramPoint.cpp
  ../Analysis/PseudoConstantAnalysis.cpp
  ../Analysis/ReachableCode.cpp
  ../Analysis/ThreadSafety.cpp
  ../Analysis/ThreadSafetyCommon.cpp
  ../Analysis/ThreadSafetyLogical.cpp
  ../Analysis/ThreadSafetyTIL.cpp
  ../Analysis/UninitializedValues.cpp

  LINK_LIBS
  clangCodeGen
  clangBasic
  clangLex
  clangSema
  )

# MERGE REVIEW:
#  ClangAttrParsedAttrList  # MS Change - AST will refer to Sema headers
#  ClangAttrParsedAttrKinds # MS Change - AST will refer to Sema headers
#  ClangAttrParsedAttrImpl  # MS Change - AST will refer to Sema headers
