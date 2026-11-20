///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcrewriteautodiff.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements -generate-differentials: synthesises forward and backward      //
// mode auto-differentiation variants of functions annotated with the        //
// [[dxc::AutoDiff(...)]] attribute.                                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxcrewriteautodiff.h"
#include "dxcrewriteautodiffir.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace llvm;

namespace {

bool isInactiveParameter(const ParmVarDecl *P) {
  return hlsl::autodiff::isADInactiveParameter(P);
}

const HLSLAutoDiffAttr *getAutoDiffAttr(const FunctionDecl *FD) {
  if (!FD)
    return nullptr;
  for (const FunctionDecl *Redecl : FD->redecls())
    if (const auto *Attr = Redecl->getAttr<HLSLAutoDiffAttr>())
      return Attr;
  return nullptr;
}

std::string printType(QualType Type, const PrintingPolicy &Policy) {
  std::string Result;
  raw_string_ostream OS(Result);
  Type.print(OS, Policy);
  OS.flush();
  return Result;
}

std::string printAutoDiffParameterType(const FunctionDecl *FD,
                                       const ParmVarDecl *Parameter,
                                       const PrintingPolicy &Policy) {
  const auto *Method = dyn_cast<CXXMethodDecl>(FD);
  const auto *Record =
      Parameter->getType().getNonReferenceType()->getAs<RecordType>();
  if (Method && Record &&
      Record->getDecl()->getCanonicalDecl() ==
          Method->getParent()->getCanonicalDecl())
    return "::" + Method->getParent()->getQualifiedNameAsString();
  return printType(Parameter->getType(), Policy);
}

unsigned getComponentCount(QualType Type) {
  if (hlsl::IsHLSLVecType(Type))
    return hlsl::GetHLSLVecSize(Type);
  if (hlsl::IsHLSLMatType(Type)) {
    uint32_t Rows = 0;
    uint32_t Columns = 0;
    hlsl::GetHLSLMatRowColCount(Type, Rows, Columns);
    return Rows * Columns;
  }
  if (const auto *Vector =
          dyn_cast<VectorType>(Type.getCanonicalType().getTypePtr()))
    return Vector->getNumElements();
  if (const auto *Record = Type->getAs<RecordType>()) {
    unsigned Count = 0;
    for (const FieldDecl *Field : Record->getDecl()->fields())
      Count += getComponentCount(Field->getType());
    return Count;
  }
  if (const auto *Array =
          dyn_cast<ConstantArrayType>(Type.getCanonicalType().getTypePtr()))
    return Array->getSize().getLimitedValue() *
           getComponentCount(Array->getElementType());
  return 1;
}

char getComponentName(unsigned Index) {
  static const char Names[] = {'x', 'y', 'z', 'w'};
  return Index < 4 ? Names[Index] : 'x';
}

struct ActiveContextInfo {
  QualType Type;
  std::string Name;
  SmallVector<const ParmVarDecl *, 4> Parameters;
};

// ---------------------------------------------------------------------------
// Intrinsic classification
// ---------------------------------------------------------------------------

// Map a differentiable HLSL math intrinsic to the corresponding builder used
// by the backward-mode runtime. The forward-mode runtime overloads the
// standard intrinsic names directly on Value<T>, so no mapping is required
// there.
//
// The list mirrors the differentiable-by-design entries in
// utils/hct/gen_intrin_main.txt: trigonometric functions, exponentials,
// logarithms, roots, common smooth algebraic operations, geometric
// operations, and matrix products. Intrinsics that are piecewise constant
// (sign, step) are listed: their derivative is zero almost everywhere and
// the runtime library implements them with the subgradient convention.
StringRef GetBackwardIntrinsicBuilder(StringRef Name) {
  return StringSwitch<StringRef>(Name)
      // Trigonometry.
      .Case("sin", "sinExpr")
      .Case("cos", "cosExpr")
      .Case("tan", "tanExpr")
      .Case("asin", "asinExpr")
      .Case("acos", "acosExpr")
      .Case("atan", "atanExpr")
      .Case("atan2", "atan2Expr")
      .Case("sinh", "sinhExpr")
      .Case("cosh", "coshExpr")
      .Case("tanh", "tanhExpr")
      // Exponentials and logarithms.
      .Case("exp", "expExpr")
      .Case("exp2", "exp2Expr")
      .Case("log", "logExpr")
      .Case("log2", "log2Expr")
      .Case("log10", "log10Expr")
      .Case("pow", "power")
      // Roots, reciprocals.
      .Case("sqrt", "sqrtExpr")
      .Case("rsqrt", "rsqrtExpr")
      .Case("rcp", "rcpExpr")
      // Smooth algebraic.
      .Case("abs", "absExpr")
      .Case("saturate", "saturateExpr")
      .Case("clamp", "clampExpr")
      .Case("max", "maxExpr")
      .Case("min", "minExpr")
      .Case("lerp", "lerpExpr")
      .Case("mad", "madExpr")
      .Case("fma", "fmaExpr")
      .Case("smoothstep", "smoothstepExpr")
      // Piecewise constant (subgradient).
      .Case("sign", "signExpr")
      .Case("step", "stepExpr")
      .Case("floor", "floorExpr")
      .Case("ceil", "ceilExpr")
      .Case("round", "roundExpr")
      .Case("trunc", "truncExpr")
      .Case("frac", "fracExpr")
      .Case("fmod", "fmodExpr")
      .Case("modf", "modfExpr")
      // Conversion-style numerics.
      .Case("degrees", "degreesExpr")
      .Case("radians", "radiansExpr")
      // Geometric.
      .Case("dot", "dotExpr")
      .Case("cross", "crossExpr")
      .Case("length", "lengthExpr")
      .Case("distance", "distanceExpr")
      .Case("normalize", "normalizeExpr")
      .Case("reflect", "reflectExpr")
      .Case("refract", "refractExpr")
      .Case("faceforward", "faceforwardExpr")
      // Matrix.
      .Case("mul", "mulExpr")
      .Case("determinant", "determinantExpr")
      .Case("transpose", "transposeExpr")
      // Misc smooth.
      .Case("lit", "litExpr")
      .Case("dst", "dstExpr")
      .Case("ldexp", "ldexpExpr")
      .Default(StringRef());
}

// Intrinsics that are well-defined but mathematically not differentiable.
// Using one of them in the body of a function annotated with [[dxc::AutoDiff]]
// causes the generated forward / backward function to be a stub that fails
// at compile time with a _Static_assert.
//
// The list is intentionally explicit so that "unknown intrinsic" remains a
// distinct failure category (also stubbed, but with a different message).
const char *GetNonDifferentiableReason(StringRef Name) {
  return StringSwitch<const char *>(Name)
      // Predicates: bool return.
      .Case("any", "predicate 'any' is not differentiable")
      .Case("all", "predicate 'all' is not differentiable")
      .Case("isfinite", "predicate 'isfinite' is not differentiable")
      .Case("isinf", "predicate 'isinf' is not differentiable")
      .Case("isnan", "predicate 'isnan' is not differentiable")
      .Case("isnormal", "predicate 'isnormal' is not differentiable")
      // Bit-cast / reinterpret.
      .Case("asfloat", "bit-cast 'asfloat' is not differentiable")
      .Case("asfloat16", "bit-cast 'asfloat16' is not differentiable")
      .Case("asint", "bit-cast 'asint' is not differentiable")
      .Case("asint16", "bit-cast 'asint16' is not differentiable")
      .Case("asuint", "bit-cast 'asuint' is not differentiable")
      .Case("asuint16", "bit-cast 'asuint16' is not differentiable")
      .Case("asdouble", "bit-cast 'asdouble' is not differentiable")
      .Case("f16tof32", "bit-cast 'f16tof32' is not differentiable")
      .Case("f32tof16", "bit-cast 'f32tof16' is not differentiable")
      // Bit manipulation.
      .Case("countbits", "bit-op 'countbits' is not differentiable")
      .Case("firstbithigh", "bit-op 'firstbithigh' is not differentiable")
      .Case("firstbitlow", "bit-op 'firstbitlow' is not differentiable")
      .Case("reversebits", "bit-op 'reversebits' is not differentiable")
      // Integer-typed.
      .Case("D3DCOLORtoUBYTE4",
            "integer-valued 'D3DCOLORtoUBYTE4' is not differentiable")
      .Case("msad4", "integer-valued 'msad4' is not differentiable")
      .Case("dot4add_i8packed",
            "integer-valued 'dot4add_i8packed' is not differentiable")
      .Case("dot4add_u8packed",
            "integer-valued 'dot4add_u8packed' is not differentiable")
      .Case("dot2add", "'dot2add' is not differentiable")
      .Case("unpack_s8s32",
            "integer-valued 'unpack_s8s32' is not differentiable")
      .Case("unpack_u8u32",
            "integer-valued 'unpack_u8u32' is not differentiable")
      .Case("AddUint64", "'AddUint64' is not differentiable")
      // Side-effecting / control.
      .Case("clip", "side-effecting 'clip' is not differentiable")
      .Case("abort", "side-effecting 'abort' is not differentiable")
      .Case("printf", "side-effecting 'printf' is not differentiable")
      .Case("DebugBreak", "side-effecting 'DebugBreak' is not differentiable")
      .Case("source_mark", "side-effecting 'source_mark' is not differentiable")
      .Case("Barrier", "barrier 'Barrier' is not differentiable")
      .Case("AllMemoryBarrier",
            "barrier 'AllMemoryBarrier' is not differentiable")
      .Case("AllMemoryBarrierWithGroupSync",
            "barrier 'AllMemoryBarrierWithGroupSync' is not differentiable")
      .Case("DeviceMemoryBarrier",
            "barrier 'DeviceMemoryBarrier' is not differentiable")
      .Case("DeviceMemoryBarrierWithGroupSync",
            "barrier 'DeviceMemoryBarrierWithGroupSync' is not differentiable")
      .Case("GroupMemoryBarrier",
            "barrier 'GroupMemoryBarrier' is not differentiable")
      .Case("GroupMemoryBarrierWithGroupSync",
            "barrier 'GroupMemoryBarrierWithGroupSync' is not differentiable")
      // Atomics / interlocked.
      .Case("InterlockedAdd", "atomic 'InterlockedAdd' is not differentiable")
      .Case("InterlockedAnd", "atomic 'InterlockedAnd' is not differentiable")
      .Case("InterlockedOr", "atomic 'InterlockedOr' is not differentiable")
      .Case("InterlockedXor", "atomic 'InterlockedXor' is not differentiable")
      .Case("InterlockedMin", "atomic 'InterlockedMin' is not differentiable")
      .Case("InterlockedMax", "atomic 'InterlockedMax' is not differentiable")
      .Case("InterlockedExchange",
            "atomic 'InterlockedExchange' is not differentiable")
      .Case("InterlockedCompareStore",
            "atomic 'InterlockedCompareStore' is not differentiable")
      .Case("InterlockedCompareExchange",
            "atomic 'InterlockedCompareExchange' is not differentiable")
      .Case("InterlockedCompareStoreFloatBitwise",
            "atomic 'InterlockedCompareStoreFloatBitwise' "
            "is not differentiable")
      .Case("InterlockedCompareExchangeFloatBitwise",
            "atomic 'InterlockedCompareExchangeFloatBitwise' "
            "is not differentiable")
      // Derivative-of intrinsics are discontinuous quad-level operations.
      .Case("ddx", "quad-derivative 'ddx' is not differentiable")
      .Case("ddx_coarse", "quad-derivative 'ddx_coarse' is not differentiable")
      .Case("ddx_fine", "quad-derivative 'ddx_fine' is not differentiable")
      .Case("ddy", "quad-derivative 'ddy' is not differentiable")
      .Case("ddy_coarse", "quad-derivative 'ddy_coarse' is not differentiable")
      .Case("ddy_fine", "quad-derivative 'ddy_fine' is not differentiable")
      .Case("fwidth", "quad-derivative 'fwidth' is not differentiable")
      .Case("EvaluateAttributeAtSample",
            "interpolation 'EvaluateAttributeAtSample' is not differentiable")
      .Case("EvaluateAttributeCentroid",
            "interpolation 'EvaluateAttributeCentroid' is not differentiable")
      .Case("EvaluateAttributeSnapped",
            "interpolation 'EvaluateAttributeSnapped' is not differentiable")
      .Case("GetAttributeAtVertex",
            "interpolation 'GetAttributeAtVertex' is not differentiable")
      // Wave/quad ops.
      .Case("WaveActiveAllTrue",
            "wave-op 'WaveActiveAllTrue' is not differentiable")
      .Case("WaveActiveAnyTrue",
            "wave-op 'WaveActiveAnyTrue' is not differentiable")
      .Case("WaveActiveBallot",
            "wave-op 'WaveActiveBallot' is not differentiable")
      .Case("WaveActiveCountBits",
            "wave-op 'WaveActiveCountBits' is not differentiable")
      .Case("WaveGetLaneCount",
            "wave-op 'WaveGetLaneCount' is not differentiable")
      .Case("WaveGetLaneIndex",
            "wave-op 'WaveGetLaneIndex' is not differentiable")
      .Case("WaveIsFirstLane",
            "wave-op 'WaveIsFirstLane' is not differentiable")
      .Case("WaveMatch", "wave-op 'WaveMatch' is not differentiable")
      .Case("WaveMultiPrefixCountBits",
            "wave-op 'WaveMultiPrefixCountBits' is not differentiable")
      .Case("WavePrefixCountBits",
            "wave-op 'WavePrefixCountBits' is not differentiable")
      .Case("QuadAll", "wave-op 'QuadAll' is not differentiable")
      .Case("QuadAny", "wave-op 'QuadAny' is not differentiable")
      .Case("IsHelperLane", "'IsHelperLane' is not differentiable")
      // Raytracing / mesh / system-value queries.
      .Case("TraceRay", "raytracing 'TraceRay' is not differentiable")
      .Case("CallShader", "raytracing 'CallShader' is not differentiable")
      .Case("ReportHit", "raytracing 'ReportHit' is not differentiable")
      .Case("IgnoreHit", "raytracing 'IgnoreHit' is not differentiable")
      .Case("AcceptHitAndEndSearch",
            "raytracing 'AcceptHitAndEndSearch' is not differentiable")
      .Case("AllocateRayQuery",
            "raytracing 'AllocateRayQuery' is not differentiable")
      .Case("DispatchRaysIndex",
            "system-value 'DispatchRaysIndex' is not differentiable")
      .Case("DispatchRaysDimensions",
            "system-value 'DispatchRaysDimensions' is not differentiable")
      .Case("RayFlags", "system-value 'RayFlags' is not differentiable")
      .Case("RayTMin", "system-value 'RayTMin' is not differentiable")
      .Case("RayTCurrent", "system-value 'RayTCurrent' is not differentiable")
      .Case("HitKind", "system-value 'HitKind' is not differentiable")
      .Case("InstanceID", "system-value 'InstanceID' is not differentiable")
      .Case("InstanceIndex",
            "system-value 'InstanceIndex' is not differentiable")
      .Case("PrimitiveIndex",
            "system-value 'PrimitiveIndex' is not differentiable")
      .Case("GeometryIndex",
            "system-value 'GeometryIndex' is not differentiable")
      .Case("ClusterID", "system-value 'ClusterID' is not differentiable")
      .Case("WorldRayOrigin",
            "system-value 'WorldRayOrigin' is not differentiable")
      .Case("WorldRayDirection",
            "system-value 'WorldRayDirection' is not differentiable")
      .Case("ObjectRayOrigin",
            "system-value 'ObjectRayOrigin' is not differentiable")
      .Case("ObjectRayDirection",
            "system-value 'ObjectRayDirection' is not differentiable")
      .Case("ObjectToWorld",
            "system-value 'ObjectToWorld' is not differentiable")
      .Case("ObjectToWorld3x4",
            "system-value 'ObjectToWorld3x4' is not differentiable")
      .Case("ObjectToWorld4x3",
            "system-value 'ObjectToWorld4x3' is not differentiable")
      .Case("WorldToObject",
            "system-value 'WorldToObject' is not differentiable")
      .Case("WorldToObject3x4",
            "system-value 'WorldToObject3x4' is not differentiable")
      .Case("WorldToObject4x3",
            "system-value 'WorldToObject4x3' is not differentiable")
      .Case("TriangleObjectPositions",
            "raytracing 'TriangleObjectPositions' is not differentiable")
      .Case("GetRemainingRecursionLevels",
            "system-value 'GetRemainingRecursionLevels' is not differentiable")
      .Case("GetGroupWaveCount",
            "system-value 'GetGroupWaveCount' is not differentiable")
      .Case("GetGroupWaveIndex",
            "system-value 'GetGroupWaveIndex' is not differentiable")
      .Case("GetRenderTargetSampleCount",
            "system-value 'GetRenderTargetSampleCount' is not differentiable")
      .Case("GetRenderTargetSamplePosition",
            "system-value 'GetRenderTargetSamplePosition' "
            "is not differentiable")
      .Case("GetSamplePosition",
            "system-value 'GetSamplePosition' is not differentiable")
      .Case("CheckAccessFullyMapped",
            "'CheckAccessFullyMapped' is not differentiable")
      .Case("CreateResourceFromHeap",
            "'CreateResourceFromHeap' is not differentiable")
      .Case("DispatchMesh", "mesh-shader 'DispatchMesh' is not differentiable")
      .Case("SetMeshOutputCounts",
            "mesh-shader 'SetMeshOutputCounts' is not differentiable")
      // Tessellator helpers.
      .Case("Process2DQuadTessFactorsAvg",
            "tessellator helper is not differentiable")
      .Case("Process2DQuadTessFactorsMax",
            "tessellator helper is not differentiable")
      .Case("Process2DQuadTessFactorsMin",
            "tessellator helper is not differentiable")
      .Case("ProcessIsolineTessFactors",
            "tessellator helper is not differentiable")
      .Case("ProcessQuadTessFactorsAvg",
            "tessellator helper is not differentiable")
      .Case("ProcessQuadTessFactorsMax",
            "tessellator helper is not differentiable")
      .Case("ProcessQuadTessFactorsMin",
            "tessellator helper is not differentiable")
      .Case("ProcessTriTessFactorsAvg",
            "tessellator helper is not differentiable")
      .Case("ProcessTriTessFactorsMax",
            "tessellator helper is not differentiable")
      .Case("ProcessTriTessFactorsMin",
            "tessellator helper is not differentiable")
      .Case("frexp", "'frexp' is not differentiable")
      .Case("sincos", "side-effecting 'sincos' is not differentiable; "
                      "use sin and cos separately")
      // Texture sampling family. Any name beginning with "tex", "Sample",
      // "Load", "Gather", "CalculateLevelOfDetail" is filtered below in
      // IsTextureLikeIntrinsic for completeness; the explicit entries here
      // pin down the historical d3d9-style helpers.
      .Case("tex1D", "texture sample is not differentiable")
      .Case("tex2D", "texture sample is not differentiable")
      .Case("tex3D", "texture sample is not differentiable")
      .Case("texCUBE", "texture sample is not differentiable")
      .Case("tex1Dbias", "texture sample is not differentiable")
      .Case("tex1Dgrad", "texture sample is not differentiable")
      .Case("tex1Dlod", "texture sample is not differentiable")
      .Case("tex1Dproj", "texture sample is not differentiable")
      .Case("tex2Dbias", "texture sample is not differentiable")
      .Case("tex2Dgrad", "texture sample is not differentiable")
      .Case("tex2Dlod", "texture sample is not differentiable")
      .Case("tex2Dproj", "texture sample is not differentiable")
      .Case("tex3Dbias", "texture sample is not differentiable")
      .Case("tex3Dgrad", "texture sample is not differentiable")
      .Case("tex3Dlod", "texture sample is not differentiable")
      .Case("tex3Dproj", "texture sample is not differentiable")
      .Case("texCUBEbias", "texture sample is not differentiable")
      .Case("texCUBEgrad", "texture sample is not differentiable")
      .Case("texCUBElod", "texture sample is not differentiable")
      .Case("texCUBEproj", "texture sample is not differentiable")
      .Default(nullptr);
}

// Textures / linear algebra intrinsics carry __builtin_ or Sample / Load /
// Gather / CalculateLevelOfDetail prefixes. Catch them by name pattern so
// new entries in gen_intrin_main.txt are non-differentiable by default.
bool IsTextureLikeIntrinsic(StringRef Name) {
  return Name.startswith("Sample") || Name.startswith("Load") ||
         Name.startswith("Gather") ||
         Name.startswith("CalculateLevelOfDetail") ||
         Name.startswith("__builtin_LinAlg");
}

} // anonymous namespace

namespace {

// Translate a function body expression into a textual representation in either
// forward or backward auto-diff form. The translation is intentionally
// conservative: a documented subset of HLSL is recognised and translated; any
// construct that falls outside that subset is rendered with /*TODO*/ markers
// and the original tokens so that the generated output is still useful to a
// human reviewer.
class AutoDiffEmitter {
public:
  enum Mode { Fwd, Bwd };

  AutoDiffEmitter(Mode M, StringRef ElemType, raw_ostream &OS,
                  const PrintingPolicy &P, const ASTContext &Ctx)
      : M(M), ElemType(ElemType), OS(OS), Policy(P), Ctx(Ctx) {}

  // Whether emission of the current function has encountered a construct that
  // is fundamentally not differentiable. Such constructs cause the emitter to
  // wrap the function body in a `_Static_assert(false, ...)` stub.
  bool sawNonDifferentiable() const { return NonDifferentiable; }
  StringRef nonDifferentiableReason() const { return Reason; }

  // Emit `<expr>` translated into the chosen mode.
  void emitExpr(const Expr *E) {
    // `[[dxc::NoDiff]] <sub-expr>` is recorded as a side-table mark on the
    // sub-expression itself by the parser. Honour it here: copy the
    // sub-expression verbatim rather than running it through the
    // mode-specific translator. We probe both the original Expr and the
    // ParenImpCast-stripped form so the user can write the attribute either
    // in front of a bare call or in front of a parenthesised sub-expression.
    const Expr *Orig = E;
    if (Orig && (Ctx.isHLSLNoDiffExpr(Orig) ||
                 Ctx.isHLSLNoDiffExpr(Orig->IgnoreParenImpCasts()))) {
      Orig->IgnoreParenImpCasts()->printPretty(OS, nullptr, Policy);
      return;
    }
    E = E ? E->IgnoreParenImpCasts() : nullptr;
    if (!E) {
      OS << "/*null*/";
      return;
    }
    if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
      emitBinaryOp(BO);
      return;
    }
    if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
      emitUnaryOp(UO);
      return;
    }
    if (const auto *CE = dyn_cast<CallExpr>(E)) {
      emitCall(CE);
      return;
    }
    if (const auto *DRE = dyn_cast<DeclRefExpr>(E)) {
      emitDeclRef(DRE);
      return;
    }
    if (const auto *ME = dyn_cast<MemberExpr>(E)) {
      emitMember(ME);
      return;
    }
    if (isa<CXXThisExpr>(E)) {
      // `this` refers to the wrapper-class instance. The wrapper inherits
      // from the original record, so member accesses through `this` resolve
      // to the inherited data members; emit it verbatim.
      OS << "this";
      return;
    }
    if (isa<ConditionalOperator>(E)) {
      // Ternary is not differentiable in a sound way unless both arms have
      // identical gradients; flag the function and emit the expression as-is
      // so the surrounding stub still parses.
      markNonDifferentiable("the ternary ?: operator is not differentiable");
      E->printPretty(OS, nullptr, Policy);
      return;
    }
    if (isa<FloatingLiteral>(E) || isa<IntegerLiteral>(E)) {
      // Literals are printed as-is; the ad library expects raw scalars in
      // many positions, so we don't wrap them in constant<T>(...).
      E->printPretty(OS, nullptr, Policy);
      return;
    }
    // Fallback: pretty print as-is so the generated function still compiles
    // approximately in the user's hand-off cases. Mark with a TODO.
    OS << "/*TODO: unsupported expr*/ ";
    E->printPretty(OS, nullptr, Policy);
  }

  // Translate a single statement. `Indent` is the leading whitespace for the
  // line(s) produced. Unrecognised statements pass through verbatim with a
  // TODO marker and flag the function as non-differentiable so it gets a
  // stub.
  void emitStmt(const Stmt *S, StringRef Indent) {
    if (!S) {
      OS << Indent << ";\n";
      return;
    }
    // [[dxc::NoDiff]] copies the substatement verbatim.
    if (const auto *AS = dyn_cast<AttributedStmt>(S)) {
      for (const Attr *A : AS->getAttrs()) {
        if (isa<HLSLNoDiffAttr>(A)) {
          const Stmt *Sub = AS->getSubStmt();
          OS << Indent;
          Sub->printPretty(OS, nullptr, Policy);
          // Bare expressions used in statement position don't print their
          // trailing semicolon themselves; add it so the generated body
          // remains syntactically well-formed.
          if (isa<Expr>(Sub))
            OS << ";";
          OS << "\n";
          return;
        }
      }
      // Other attributes: fall through and process the substatement.
      emitStmt(AS->getSubStmt(), Indent);
      return;
    }
    if (const auto *CS = dyn_cast<CompoundStmt>(S)) {
      OS << Indent << "{\n";
      SmallString<32> Nested(Indent);
      Nested += "  ";
      for (const Stmt *Child : CS->body())
        emitStmt(Child, Nested);
      OS << Indent << "}\n";
      return;
    }
    if (const auto *RS = dyn_cast<ReturnStmt>(S)) {
      OS << Indent << "return";
      if (RS->getRetValue()) {
        // Backward builders produce expression-template types. Evaluate the
        // completed graph here, returning its primal value and accumulating
        // parameter gradients in the caller-provided context.
        OS << (M == Bwd ? " compute_gradients_seeded(context, " : " ");
        emitExpr(RS->getRetValue());
        if (M == Bwd)
          OS << ", __dxc_ad_seed)";
      }
      OS << ";\n";
      return;
    }
    if (const auto *IS = dyn_cast<IfStmt>(S)) {
      // Control flow with data-dependent conditions is unsound to
      // differentiate without special handling. Flag it but still emit
      // structurally so the stub diagnostic is informative.
      markNonDifferentiable(
          "data-dependent control flow (if) is not differentiable; "
          "use [[dxc::NoDiff]] or branchless math");
      OS << Indent << "if (";
      if (IS->getCond())
        IS->getCond()->printPretty(OS, nullptr, Policy);
      OS << ")\n";
      emitStmt(IS->getThen(), Indent);
      if (IS->getElse()) {
        OS << Indent << "else\n";
        emitStmt(IS->getElse(), Indent);
      }
      return;
    }
    if (const auto *WS = dyn_cast<WhileStmt>(S)) {
      markNonDifferentiable(
          "data-dependent control flow (while) is not differentiable");
      OS << Indent << "while (";
      if (WS->getCond())
        WS->getCond()->printPretty(OS, nullptr, Policy);
      OS << ")\n";
      emitStmt(WS->getBody(), Indent);
      return;
    }
    if (const auto *FS = dyn_cast<ForStmt>(S)) {
      markNonDifferentiable(
          "data-dependent control flow (for) is not differentiable");
      OS << Indent << "for (";
      if (FS->getInit())
        FS->getInit()->printPretty(OS, nullptr, Policy);
      else
        OS << ";";
      if (FS->getCond()) {
        OS << " ";
        FS->getCond()->printPretty(OS, nullptr, Policy);
      }
      OS << ";";
      if (FS->getInc()) {
        OS << " ";
        FS->getInc()->printPretty(OS, nullptr, Policy);
      }
      OS << ")\n";
      emitStmt(FS->getBody(), Indent);
      return;
    }
    if (const auto *DS = dyn_cast<DeclStmt>(S)) {
      // Declarations: translate each VarDecl, mapping its initializer
      // through the expression translator. The static type becomes
      // Value<T>/Variable<T> for the chosen mode.
      const char *Wrap = (M == Fwd) ? "Value" : "Variable";
      for (const Decl *D : DS->decls()) {
        const auto *VD = dyn_cast<VarDecl>(D);
        if (!VD) {
          markNonDifferentiable("unsupported declaration in body");
          OS << Indent;
          DS->printPretty(OS, nullptr, Policy);
          OS << "\n";
          return;
        }
        OS << Indent << Wrap << "<" << ElemType << "> " << VD->getName();
        if (VD->hasInit()) {
          OS << " = ";
          emitExpr(VD->getInit());
        }
        OS << ";\n";
      }
      return;
    }
    // ExprStmt / NullStmt and friends. Use printPretty for ExprStmt, but
    // translate the expression where possible.
    if (const auto *E = dyn_cast<Expr>(S)) {
      OS << Indent;
      emitExpr(E);
      OS << ";\n";
      return;
    }
    // Anything else: flag and pass through.
    markNonDifferentiable("unsupported statement in function body");
    OS << Indent << "/*TODO: unsupported statement*/ ";
    S->printPretty(OS, nullptr, Policy);
    OS << "\n";
  }

private:
  Mode M;
  StringRef ElemType;
  raw_ostream &OS;
  const PrintingPolicy &Policy;
  const ASTContext &Ctx;
  bool NonDifferentiable = false;
  std::string Reason;

  void markNonDifferentiable(StringRef R) {
    if (!NonDifferentiable) {
      NonDifferentiable = true;
      Reason = R.str();
    }
  }

  void emitBinaryOp(const BinaryOperator *BO) {
    // Classify operators up-front so we have a single source of truth for
    // both modes. Anything not in either list is "unknown" and produces a
    // diagnostic stub.
    BinaryOperatorKind Op = BO->getOpcode();
    const char *Fn = nullptr;
    bool Diff = true;
    switch (Op) {
    case BO_Add:
      Fn = "add";
      break;
    case BO_Sub:
      Fn = "subtract";
      break;
    case BO_Mul:
      Fn = "multiply";
      break;
    case BO_Div:
      Fn = "divide";
      break;
    case BO_Assign:
      Fn = "assign";
      break;
    case BO_AddAssign:
      Fn = "addAssign";
      break;
    case BO_SubAssign:
      Fn = "subAssign";
      break;
    case BO_MulAssign:
      Fn = "mulAssign";
      break;
    case BO_DivAssign:
      Fn = "divAssign";
      break;
    // Comparison operators return bool: not meaningfully differentiable.
    case BO_LT:
    case BO_GT:
    case BO_LE:
    case BO_GE:
    case BO_EQ:
    case BO_NE:
      markNonDifferentiable("comparison operators are not differentiable");
      Diff = false;
      break;
    // Logical operators on bool: not meaningfully differentiable.
    case BO_LAnd:
    case BO_LOr:
      markNonDifferentiable("logical operators are not differentiable");
      Diff = false;
      break;
    // Bitwise operators on integral types: not differentiable.
    case BO_And:
    case BO_Or:
    case BO_Xor:
    case BO_Shl:
    case BO_Shr:
    case BO_AndAssign:
    case BO_OrAssign:
    case BO_XorAssign:
    case BO_ShlAssign:
    case BO_ShrAssign:
      markNonDifferentiable("bitwise operators are not differentiable");
      Diff = false;
      break;
    case BO_Rem:
    case BO_RemAssign:
      markNonDifferentiable("integer remainder is not differentiable");
      Diff = false;
      break;
    default:
      break;
    }
    if (!Diff) {
      // Best-effort pass-through; the stub assertion will reject the
      // generated function at compile time anyway.
      BO->printPretty(OS, nullptr, Policy);
      return;
    }
    if (M == Fwd) {
      // Forward mode: Value<T> overloads the standard operators.
      OS << "(";
      emitExpr(BO->getLHS());
      OS << " " << BinaryOperator::getOpcodeStr(Op) << " ";
      emitExpr(BO->getRHS());
      OS << ")";
      return;
    }
    if (!Fn) {
      OS << "/*TODO: unsupported binop*/ ";
      BO->printPretty(OS, nullptr, Policy);
      return;
    }
    OS << Fn << "<" << ElemType << ">(";
    emitExpr(BO->getLHS());
    OS << ", ";
    emitExpr(BO->getRHS());
    OS << ")";
  }

  void emitUnaryOp(const UnaryOperator *UO) {
    UnaryOperatorKind Op = UO->getOpcode();
    // Classify the unary operator first.
    switch (Op) {
    case UO_Plus:
    case UO_Minus:
      break; // differentiable
    case UO_LNot:
      markNonDifferentiable("logical-not is not differentiable");
      UO->printPretty(OS, nullptr, Policy);
      return;
    case UO_Not:
      markNonDifferentiable("bitwise-not is not differentiable");
      UO->printPretty(OS, nullptr, Policy);
      return;
    case UO_PreInc:
    case UO_PreDec:
    case UO_PostInc:
    case UO_PostDec:
      markNonDifferentiable(
          "increment/decrement operators are not differentiable");
      UO->printPretty(OS, nullptr, Policy);
      return;
    default:
      break;
    }
    if (M == Fwd) {
      OS << UnaryOperator::getOpcodeStr(Op);
      OS << "(";
      emitExpr(UO->getSubExpr());
      OS << ")";
      return;
    }
    if (Op == UO_Minus) {
      OS << "negate<" << ElemType << ">(";
      emitExpr(UO->getSubExpr());
      OS << ")";
      return;
    }
    if (Op == UO_Plus) {
      // Unary plus is a no-op even in backward mode.
      emitExpr(UO->getSubExpr());
      return;
    }
    OS << "/*TODO: unsupported unop*/ ";
    UO->printPretty(OS, nullptr, Policy);
  }

  void emitCall(const CallExpr *CE) {
    const FunctionDecl *Callee = CE->getDirectCallee();
    std::string NameStorage = Callee ? Callee->getNameAsString() : "";
    StringRef Name = NameStorage;

    // Reject known-non-differentiable intrinsics, plus the texture / linalg
    // intrinsic families recognised by prefix.
    if (const char *R = GetNonDifferentiableReason(Name))
      markNonDifferentiable(R);
    else if (IsTextureLikeIntrinsic(Name))
      markNonDifferentiable("texture / linear-algebra intrinsic '" +
                            std::string(Name) + "' is not differentiable");

    if (M == Fwd) {
      // Forward mode: the ad library overloads the standard names.
      OS << Name << "(";
      for (unsigned I = 0, N = CE->getNumArgs(); I < N; ++I) {
        if (I)
          OS << ", ";
        emitExpr(CE->getArg(I));
      }
      OS << ")";
      return;
    }
    // Backward mode: map to a builder, when known.
    StringRef Mapped = GetBackwardIntrinsicBuilder(Name);
    if (Mapped.empty()) {
      // Unknown / non-differentiable callee: still write the original call
      // text so that, when paired with the _Static_assert stub, the
      // diagnostic includes the offending name.
      if (!NonDifferentiable)
        markNonDifferentiable("unknown callee '" + std::string(Name) +
                              "' has no auto-diff builder");
      OS << "/*non-differentiable call " << Name << "*/ ";
      CE->printPretty(OS, nullptr, Policy);
      return;
    }
    OS << Mapped << "<" << ElemType << ">(";
    for (unsigned I = 0, N = CE->getNumArgs(); I < N; ++I) {
      if (I)
        OS << ", ";
      emitExpr(CE->getArg(I));
    }
    OS << ")";
  }

  void emitDeclRef(const DeclRefExpr *DRE) {
    StringRef Name = DRE->getDecl()->getName();
    if (M == Fwd) {
      OS << Name;
      return;
    }
    // In backward mode, parameters are referenced through their _expr
    // VariableExpr wrappers declared at the top of the body.
    if (const auto *P = dyn_cast<ParmVarDecl>(DRE->getDecl())) {
      if (isInactiveParameter(P)) {
        OS << Name;
        return;
      }
      OS << Name << "_expr";
      return;
    }
    OS << Name;
  }

  // Translate `obj.member` / `this->member`. The wrapper class inherits from
  // the user's record, so member accesses are still valid against `this`.
  // The translated body treats the underlying member as an opaque scalar of
  // the chosen element type and emits the access verbatim; the autodiff
  // library is expected to provide the conversions that make this type-check
  // (typically Value<T> * T or constant<T>(...) wrappers).
  void emitMember(const MemberExpr *ME) {
    const Expr *Base = ME->getBase();
    if (Base && !isa<CXXThisExpr>(Base->IgnoreParenImpCasts())) {
      emitExpr(Base);
      OS << (ME->isArrow() ? "->" : ".");
    } else if (Base && ME->isArrow()) {
      OS << "this->";
    } else {
      OS << "this.";
    }
    if (const NamedDecl *MD = ME->getMemberDecl())
      OS << MD->getName();
  }
};

// Lower a typed, straight-line AD plan. Forward mode preserves named locals
// and assignments. Backward mode expands immutable local bindings into the
// final expression graph so runtime expression-template types never need a
// source-level local type.
class TypedAutoDiffEmitter {
public:
  TypedAutoDiffEmitter(AutoDiffEmitter::Mode M, StringRef ElemType,
                       raw_ostream &OS, const PrintingPolicy &P)
      : M(M), ElemType(ElemType), OS(OS), Policy(P) {}

  bool sawNonDifferentiable() const { return NonDifferentiable; }
  StringRef nonDifferentiableReason() const { return Reason; }

  void emitPlan(const hlsl::autodiff::ADFunctionPlan &Plan) {
    if (M == AutoDiffEmitter::Bwd)
      for (const VarDecl *Local : Plan.PrimalLocals)
        OS << "    " << printType(Local->getType(), Policy) << " "
           << Local->getName() << ";\n";
    for (const hlsl::autodiff::ADStmt &S : Plan.Statements) {
      switch (S.K) {
      case hlsl::autodiff::ADStmt::Kind::Declare:
        if (M == AutoDiffEmitter::Fwd) {
          std::string LocalType =
              printType(S.Binding->SourceDecl->getType(), Policy);
          OS << "    Value<" << LocalType << "> "
             << S.Binding->SourceDecl->getName();
          if (!S.Value) {
            OS << ";\n";
            break;
          }
          OS << " = ";
          if (S.Value->Value.Activity == hlsl::autodiff::ADActivity::Inactive) {
            OS << "Value<" << LocalType << ">::CreateValue(";
            emitPrimalExpr(S.Value);
            OS << ")";
          } else {
            emitExpr(S.Value);
          }
          OS << ";\n";
        }
        break;
      case hlsl::autodiff::ADStmt::Kind::Assign:
        if (M == AutoDiffEmitter::Fwd) {
          OS << "    " << S.Binding->SourceDecl->getName() << " = ";
          const auto *Parameter =
              dyn_cast<ParmVarDecl>(S.Binding->SourceDecl);
          if (Parameter && isInactiveParameter(Parameter))
            emitPrimalExpr(S.Value);
          else
            emitExpr(S.Value);
          OS << ";\n";
        }
        break;
      case hlsl::autodiff::ADStmt::Kind::Expression:
        OS << "    ";
        emitPrimalExpr(S.Value);
        OS << ";\n";
        break;
      case hlsl::autodiff::ADStmt::Kind::PrimalLoop:
        OS << "    ";
        S.SourceStmt->printPretty(OS, nullptr, Policy);
        OS << "\n";
        break;
      case hlsl::autodiff::ADStmt::Kind::ActiveLoop:
        markNonDifferentiable(
            "runtime loops mutating active state require backward mode");
        break;
      case hlsl::autodiff::ADStmt::Kind::Return:
        if (S.Value->Value.Activity == hlsl::autodiff::ADActivity::Inactive) {
          if (M == AutoDiffEmitter::Bwd)
            OS << "    context.zeroGradients();\n    return ";
          else
            OS << "    return Value<" << ElemType << ">::CreateValue(";
          emitPrimalExpr(S.Value);
          if (M == AutoDiffEmitter::Fwd)
            OS << ")";
          OS << ";\n";
        } else {
          OS << "    return ";
          if (M == AutoDiffEmitter::Bwd)
            OS << "compute_gradients_seeded(context, ";
          emitExpr(S.Value);
          if (M == AutoDiffEmitter::Bwd)
            OS << ", __dxc_ad_seed)";
          OS << ";\n";
        }
        break;
      }
    }
  }

private:
  AutoDiffEmitter::Mode M;
  StringRef ElemType;
  raw_ostream &OS;
  const PrintingPolicy &Policy;
  bool NonDifferentiable = false;
  std::string Reason;

  void markNonDifferentiable(StringRef R) {
    if (!NonDifferentiable) {
      NonDifferentiable = true;
      Reason = R.str();
    }
  }

  void emitPrimalExpr(const hlsl::autodiff::ADExpr *E) {
    using ExprKind = hlsl::autodiff::ADExpr::Kind;
    switch (E->K) {
    case ExprKind::Literal:
      E->SourceExpr->printPretty(OS, nullptr, Policy);
      return;
    case ExprKind::DeclRef:
      OS << E->SourceDecl->getName();
      if (const auto *P = dyn_cast<ParmVarDecl>(E->SourceDecl)) {
        if (!isInactiveParameter(P))
          OS << ".value";
      }
      return;
    case ExprKind::LocalRef:
      if (M == AutoDiffEmitter::Bwd) {
        emitPrimalExpr(E->Binding->Value);
      } else {
        const auto *Parameter =
            dyn_cast<ParmVarDecl>(E->Binding->SourceDecl);
        OS << E->Binding->SourceDecl->getName();
        if (!Parameter || !isInactiveParameter(Parameter))
          OS << ".value";
      }
      return;
    case ExprKind::PrimalLocal:
      OS << E->SourceDecl->getName();
      return;
    case ExprKind::This:
      OS << "this";
      return;
    case ExprKind::Member: {
      const auto *ME = cast<MemberExpr>(E->SourceExpr);
      const hlsl::autodiff::ADExpr *Base = E->Operands.front();
      if (isa<CXXThisExpr>(Base->SourceExpr->IgnoreParenImpCasts()))
        OS << (ME->isArrow() ? "this->" : "this.");
      else {
        emitPrimalExpr(Base);
        OS << (ME->isArrow() ? "->" : ".");
      }
      OS << E->SourceDecl->getName();
      return;
    }
    case ExprKind::Swizzle: {
      emitPrimalExpr(E->Operands.front());
      OS << "."
         << cast<HLSLVectorElementExpr>(E->SourceExpr)->getAccessor().getName();
      return;
    }
    case ExprKind::Subscript:
      emitPrimalExpr(E->Operands[0]);
      OS << "[";
      emitPrimalExpr(E->Operands[1]);
      OS << "]";
      return;
    case ExprKind::AggregateConstruct:
      if (isa<InitListExpr>(E->SourceExpr))
        OS << "{";
      else {
        E->Value.PrimalType.print(OS, Policy);
        OS << "(";
      }
      for (unsigned I = 0; I < E->Operands.size(); ++I) {
        if (I)
          OS << ", ";
        emitPrimalExpr(E->Operands[I]);
      }
      OS << (isa<InitListExpr>(E->SourceExpr) ? "}" : ")");
      return;
    case ExprKind::Cast:
      OS << "(";
      E->Value.PrimalType.print(OS, Policy);
      OS << ")";
      emitPrimalExpr(E->Operands.front());
      return;
    case ExprKind::Unary:
      OS << UnaryOperator::getOpcodeStr(E->UnaryOpcode) << "(";
      emitPrimalExpr(E->Operands.front());
      OS << ")";
      return;
    case ExprKind::Binary:
      OS << "(";
      emitPrimalExpr(E->Operands[0]);
      OS << " " << BinaryOperator::getOpcodeStr(E->BinaryOpcode) << " ";
      emitPrimalExpr(E->Operands[1]);
      OS << ")";
      return;
    case ExprKind::Conditional:
      OS << "(";
      emitPrimalExpr(E->Operands[0]);
      OS << " ? ";
      emitPrimalExpr(E->Operands[1]);
      OS << " : ";
      emitPrimalExpr(E->Operands[2]);
      OS << ")";
      return;
    case ExprKind::Call:
      if (E->Receiver) {
        emitPrimalExpr(E->Receiver);
        OS << ".";
      }
      OS << E->Callee->getName() << "(";
      for (unsigned I = 0; I < E->Operands.size(); ++I) {
        if (I)
          OS << ", ";
        emitPrimalExpr(E->Operands[I]);
      }
      OS << ")";
      return;
    }
  }

  void emitExpr(const hlsl::autodiff::ADExpr *E) {
    using Activity = hlsl::autodiff::ADActivity;
    using ExprKind = hlsl::autodiff::ADExpr::Kind;

    if (E->Value.Activity == Activity::Inactive) {
      if (M == AutoDiffEmitter::Fwd) {
        OS << "Value<";
        E->Value.PrimalType.print(OS, Policy);
        OS << ">::CreateValue(";
      }
      emitPrimalExpr(E);
      if (M == AutoDiffEmitter::Fwd)
        OS << ")";
      return;
    }

    switch (E->K) {
    case ExprKind::Literal:
      E->SourceExpr->printPretty(OS, nullptr, Policy);
      return;
    case ExprKind::DeclRef:
      OS << E->SourceDecl->getName();
      if (M == AutoDiffEmitter::Bwd && isa<ParmVarDecl>(E->SourceDecl))
        OS << "_expr";
      return;
    case ExprKind::LocalRef:
      if (M == AutoDiffEmitter::Bwd)
        emitExpr(E->Binding->Value);
      else
        OS << E->Binding->SourceDecl->getName();
      return;
    case ExprKind::This:
    case ExprKind::Member:
      emitPrimalExpr(E);
      return;
    case ExprKind::Swizzle:
    case ExprKind::Subscript:
    case ExprKind::AggregateConstruct:
      markNonDifferentiable(
          "vector construction or swizzle requires direct reverse lowering");
      emitPrimalExpr(E);
      return;
    case ExprKind::Cast:
      OS << "(";
      E->Value.PrimalType.print(OS, Policy);
      OS << ")";
      emitExpr(E->Operands.front());
      return;
    case ExprKind::Unary:
      if (M == AutoDiffEmitter::Fwd) {
        OS << UnaryOperator::getOpcodeStr(E->UnaryOpcode) << "(";
        emitExpr(E->Operands.front());
        OS << ")";
      } else if (E->UnaryOpcode == UO_Minus) {
        OS << "negate<" << ElemType << ">(";
        emitExpr(E->Operands.front());
        OS << ")";
      } else {
        emitExpr(E->Operands.front());
      }
      return;
    case ExprKind::Binary:
      emitBinary(E);
      return;
    case ExprKind::Conditional:
      OS << "(";
      emitPrimalExpr(E->Operands[0]);
      OS << " ? ";
      emitExpr(E->Operands[1]);
      OS << " : ";
      emitExpr(E->Operands[2]);
      OS << ")";
      return;
    case ExprKind::Call:
      emitCall(E);
      return;
    }
  }

  void emitBinary(const hlsl::autodiff::ADExpr *E) {
    if (M == AutoDiffEmitter::Fwd) {
      OS << "(";
      emitExpr(E->Operands[0]);
      OS << " " << BinaryOperator::getOpcodeStr(E->BinaryOpcode) << " ";
      emitExpr(E->Operands[1]);
      OS << ")";
      return;
    }

    const char *Builder = nullptr;
    switch (E->BinaryOpcode) {
    case BO_Add:
      Builder = "add";
      break;
    case BO_Sub:
      Builder = "subtract";
      break;
    case BO_Mul:
      Builder = "multiply";
      break;
    case BO_Div:
      Builder = "divide";
      break;
    default:
      markNonDifferentiable("unsupported binary operator in typed AD lowering");
      E->SourceExpr->printPretty(OS, nullptr, Policy);
      return;
    }
    OS << Builder << "<" << ElemType << ">(";
    emitExpr(E->Operands[0]);
    OS << ", ";
    emitExpr(E->Operands[1]);
    OS << ")";
  }

  void emitCall(const hlsl::autodiff::ADExpr *E) {
    StringRef Name = E->Callee->getName();
    if (const auto *Attr = getAutoDiffAttr(E->Callee)) {
      bool HasRequestedMode =
          M == AutoDiffEmitter::Fwd ? Attr->hasForward() : Attr->hasBackward();
      if (!HasRequestedMode)
        markNonDifferentiable(
            "callee '" + Name.str() + "' does not request " +
            (M == AutoDiffEmitter::Fwd ? "forward" : "backward") +
            "-mode auto-diff");
    }
    if (const char *R = GetNonDifferentiableReason(Name))
      markNonDifferentiable(R);
    else if (IsTextureLikeIntrinsic(Name))
      markNonDifferentiable("texture / linear-algebra intrinsic '" +
                            std::string(Name) + "' is not differentiable");

    StringRef EmittedName = Name;
    if (M == AutoDiffEmitter::Bwd) {
      EmittedName = GetBackwardIntrinsicBuilder(Name);
      if (EmittedName.empty()) {
        if (!NonDifferentiable)
          markNonDifferentiable("unknown callee '" + std::string(Name) +
                                "' has no auto-diff builder");
        OS << "/*non-differentiable call " << Name << "*/ ";
        E->SourceExpr->printPretty(OS, nullptr, Policy);
        return;
      }
    }

    OS << EmittedName;
    if (M == AutoDiffEmitter::Bwd)
      OS << "<" << ElemType << ">";
    OS << "(";
    for (unsigned I = 0; I < E->Operands.size(); ++I) {
      if (I)
        OS << ", ";
      emitExpr(E->Operands[I]);
    }
    OS << ")";
  }
};

// Direct reverse lowering handles expression graphs whose result and active
// leaf types differ. It emits primal HLSL plus explicit cotangent routing,
// avoiding the homogeneous ValueType constraint of the expression templates.
class DirectReverseEmitter {
public:
  DirectReverseEmitter(StringRef ResultType, raw_ostream &OS,
                       const PrintingPolicy &Policy,
                       ArrayRef<ActiveContextInfo> Contexts)
      : ResultType(ResultType), OS(OS), Policy(Policy), Contexts(Contexts) {}

  bool emitPlan(const hlsl::autodiff::ADFunctionPlan &Plan) {
    const hlsl::autodiff::ADExpr *Result = nullptr;
    for (const hlsl::autodiff::ADStmt &S : Plan.Statements) {
      if (S.K == hlsl::autodiff::ADStmt::Kind::Return)
        Result = S.Value;
      if (S.K == hlsl::autodiff::ADStmt::Kind::ActiveLoop) {
        assert(S.Loop && "active loop has no structured loop plan");
        for (const hlsl::autodiff::ADLoopState &State : S.Loop->States)
          RuntimeLoopPlans[State.Result] = S.Loop;
      }
    }
    if (!Result || !supports(Result))
      return false;

    for (const VarDecl *Local : Plan.PrimalLocals)
      OS << "    " << printType(Local->getType(), Policy) << " "
         << Local->getName() << ";\n";
    unsigned NextLoopID = 0;
    for (const hlsl::autodiff::ADStmt &S : Plan.Statements)
      if (S.K == hlsl::autodiff::ADStmt::Kind::Expression) {
        OS << "    ";
        emitPrimal(S.Value);
        OS << ";\n";
      } else if (S.K == hlsl::autodiff::ADStmt::Kind::PrimalLoop) {
        OS << "    ";
        S.SourceStmt->printPretty(OS, nullptr, Policy);
        OS << "\n";
      } else if (S.K == hlsl::autodiff::ADStmt::Kind::ActiveLoop) {
        assert(S.Loop && "active loop has no structured loop plan");
        const hlsl::autodiff::ADLoopPlan &Loop = *S.Loop;
        assert(Loop.Storage !=
                   hlsl::autodiff::ADLoopStorageKind::DynamicRequired &&
               "dynamic loop tape reached local-array emission");
        SmallVector<unsigned, 4> LoopIDs;
        bool NeedsBraces = Loop.States.size() > 1 || !Loop.TapeSlots.empty();
        for (const hlsl::autodiff::ADLoopState &State : Loop.States) {
          unsigned LoopID = NextLoopID++;
          LoopIDs.push_back(LoopID);
          RuntimeLoopIDs[State.Result] = LoopID;
          RuntimeLoopPlans[State.Result] = &Loop;
          NeedsBraces |= State.NeedsPrimalTape;
          if (Loop.Storage == hlsl::autodiff::ADLoopStorageKind::Recompute)
            OS << "    " << printType(State.PrimalType, Policy)
               << " __dxc_ad_loop_" << LoopID << "_initial = "
               << State.SourceDecl->getName() << ".value;\n";
          else if (State.NeedsPrimalTape)
            OS << "    " << printType(State.PrimalType, Policy)
               << " __dxc_ad_loop_" << LoopID << "_primal_tape["
               << Loop.TapeCapacity << "];\n";
        }
        if (Loop.Storage == hlsl::autodiff::ADLoopStorageKind::Static)
          for (const hlsl::autodiff::ADLoopTapeSlot &Slot : Loop.TapeSlots)
            OS << "    "
               << printType(Loop.States[Slot.StateIndex].PrimalType, Policy)
               << " __dxc_ad_loop_" << LoopIDs[Slot.StateIndex] << "_version_"
               << Slot.Version << "_primal_tape[" << Loop.TapeCapacity
               << "];\n";
        OS << "    for (uint " << Loop.Counter->getName() << " = 0; "
           << Loop.Counter->getName() << " < ";
        emitPrimal(Loop.TripCount);
        OS << "; ++" << Loop.Counter->getName() << ")";
        OS << (NeedsBraces ? " {\n" : "\n");
        for (const hlsl::autodiff::ADLoopUpdate &Update : Loop.Updates) {
          const hlsl::autodiff::ADLoopState &State =
              Loop.States[Update.TargetStateIndex];
          unsigned LoopID = LoopIDs[Update.TargetStateIndex];
            if (Loop.Storage == hlsl::autodiff::ADLoopStorageKind::Static &&
              State.NeedsPrimalTape)
            OS << "        __dxc_ad_loop_" << LoopID << "_primal_tape["
               << Loop.Counter->getName()
               << "] = " << State.SourceDecl->getName() << ".value;\n";
          OS << (NeedsBraces ? "        " : "        ")
             << State.SourceDecl->getName() << ".value ";
          switch (Update.Opcode) {
          case BO_Assign:
            OS << "= ";
            break;
          case BO_Add:
            OS << "+= ";
            break;
          case BO_Sub:
            OS << "-= ";
            break;
          case BO_Mul:
            OS << "*= ";
            break;
          case BO_Div:
            OS << "/= ";
            break;
          default:
            llvm_unreachable("validated active runtime loop operation");
          }
          emitPrimal(Update.Value);
          OS << ";\n";
          if (Loop.Storage == hlsl::autodiff::ADLoopStorageKind::Static)
            for (const hlsl::autodiff::ADLoopTapeSlot &Slot : Loop.TapeSlots)
              if (Slot.StateIndex == Update.TargetStateIndex &&
                  Slot.Version == Update.ResultVersion)
                OS << "        __dxc_ad_loop_" << LoopID << "_version_"
                   << Slot.Version << "_primal_tape["
                   << Loop.Counter->getName() << "] = "
                   << State.SourceDecl->getName() << ".value;\n";
        }
        if (NeedsBraces)
          OS << "    }\n";
      }

    OS << "    " << ResultType << " __dxc_ad_primal = ";
    emitPrimal(Result);
    OS << ";\n";
    for (const ActiveContextInfo &Context : Contexts)
      OS << "    " << Context.Name << ".zeroGradients();\n";
    emitAdjoint(Result, "__dxc_ad_seed");
    OS << "    return __dxc_ad_primal;\n";
    return true;
  }

private:
  StringRef ResultType;
  raw_ostream &OS;
  const PrintingPolicy &Policy;
  ArrayRef<ActiveContextInfo> Contexts;
  unsigned PullbackCallCount = 0;
  DenseMap<const hlsl::autodiff::ADExpr *, unsigned> RuntimeLoopIDs;
  DenseMap<const hlsl::autodiff::ADExpr *, const hlsl::autodiff::ADLoopPlan *>
      RuntimeLoopPlans;

  StringRef contextName(const ValueDecl *Decl) const {
    for (const ActiveContextInfo &Context : Contexts)
      for (const ParmVarDecl *Parameter : Context.Parameters)
        if (Parameter->getCanonicalDecl() == Decl)
          return Context.Name;
    llvm_unreachable("active parameter has no gradient context");
  }

  bool supports(const hlsl::autodiff::ADExpr *E) const {
    using Activity = hlsl::autodiff::ADActivity;
    using Kind = hlsl::autodiff::ADExpr::Kind;
    if (E->Value.Activity == Activity::Inactive)
      return true;
    switch (E->K) {
    case Kind::DeclRef:
      return isa<ParmVarDecl>(E->SourceDecl);
    case Kind::LocalRef:
      return supports(E->Binding->Value);
    case Kind::LoopStateRef:
      return true;
    case Kind::PrimalLocal:
      return true;
    case Kind::RuntimeLoopResult: {
      auto LoopIt = RuntimeLoopPlans.find(E);
      if (LoopIt == RuntimeLoopPlans.end())
        return false;
      for (const hlsl::autodiff::ADLoopState &State : LoopIt->second->States)
        if (!supports(State.InitialValue))
          return false;
      return true;
    }
    default:
      break;
    }
    hlsl::autodiff::ADPullbackRuleInfo Rule =
        hlsl::autodiff::getADPullbackRule(E);
    if (Rule.Rule == hlsl::autodiff::ADPullbackRule::Unsupported)
      return false;
    if (Rule.Rule == hlsl::autodiff::ADPullbackRule::Subscript &&
        E->Operands[1]->Value.Activity == Activity::Active)
      return false;
    if (Rule.Rule == hlsl::autodiff::ADPullbackRule::Conditional &&
        E->Operands[0]->Value.Activity == Activity::Active)
      return false;
    for (const hlsl::autodiff::ADExpr *Operand : E->Operands)
      if (!supports(Operand))
        return false;
    return true;
  }

  std::string primalText(const hlsl::autodiff::ADExpr *E) {
    std::string Text;
    raw_string_ostream Stream(Text);
    emitPrimal(E, Stream);
    Stream.flush();
    return Text;
  }

  void emitPrimal(const hlsl::autodiff::ADExpr *E) { emitPrimal(E, OS); }

  void emitPrimal(const hlsl::autodiff::ADExpr *E, raw_ostream &Out) {
    emitPrimal(E, Out, nullptr);
  }

  struct LoopPrimalContext {
    enum class Mode { StaticTape, ReplayState, ReplayVersion };
    const hlsl::autodiff::ADLoopPlan *Loop = nullptr;
    Mode M = Mode::StaticTape;
  };

  void emitPrimal(const hlsl::autodiff::ADExpr *E, raw_ostream &Out,
                  const LoopPrimalContext *LoopContext) {
    using Kind = hlsl::autodiff::ADExpr::Kind;
    switch (E->K) {
    case Kind::Literal:
      E->SourceExpr->printPretty(Out, nullptr, Policy);
      return;
    case Kind::DeclRef:
      Out << E->SourceDecl->getName();
      if (const auto *P = dyn_cast<ParmVarDecl>(E->SourceDecl))
        if (!isInactiveParameter(P))
          Out << ".value";
      return;
    case Kind::LocalRef:
      emitPrimal(E->Binding->Value, Out, LoopContext);
      return;
    case Kind::LoopStateRef: {
      if (!LoopContext) {
        Out << E->SourceDecl->getName() << ".value";
        return;
      }
      const hlsl::autodiff::ADLoopPlan &Loop = *LoopContext->Loop;
      const hlsl::autodiff::ADLoopState &State =
          Loop.States[E->LoopStateIndex];
      unsigned LoopID = RuntimeLoopIDs.lookup(State.Result);
      Out << "__dxc_ad_loop_" << LoopID;
      if (LoopContext->M == LoopPrimalContext::Mode::ReplayState) {
        Out << "_replay";
        return;
      }
      if (LoopContext->M == LoopPrimalContext::Mode::ReplayVersion)
        Out << "_version_" << E->LoopStateVersion << "_replay";
      else if (E->LoopStateVersion)
        Out << "_version_" << E->LoopStateVersion;
      if (LoopContext->M == LoopPrimalContext::Mode::StaticTape)
        Out << "_primal_tape[" << Loop.Counter->getName() << "]";
      return;
    }
    case Kind::PrimalLocal:
      Out << E->SourceDecl->getName();
      return;
    case Kind::RuntimeLoopResult:
      Out << E->SourceDecl->getName() << ".value";
      return;
    case Kind::Member: {
      const auto *Member = cast<MemberExpr>(E->SourceExpr);
      emitPrimal(E->Operands.front(), Out, LoopContext);
      Out << (Member->isArrow() ? "->" : ".") << E->SourceDecl->getName();
      return;
    }
    case Kind::Swizzle:
      emitPrimal(E->Operands.front(), Out, LoopContext);
      Out << "."
          << cast<HLSLVectorElementExpr>(E->SourceExpr)
                 ->getAccessor()
                 .getName();
      return;
    case Kind::Subscript:
      emitPrimal(E->Operands[0], Out, LoopContext);
      Out << "[";
      emitPrimal(E->Operands[1], Out, LoopContext);
      Out << "]";
      return;
    case Kind::AggregateConstruct:
      if (isa<InitListExpr>(E->SourceExpr))
        Out << "{";
      else {
        E->Value.PrimalType.print(Out, Policy);
        Out << "(";
      }
      for (unsigned I = 0; I < E->Operands.size(); ++I) {
        if (I)
          Out << ", ";
        emitPrimal(E->Operands[I], Out, LoopContext);
      }
      Out << (isa<InitListExpr>(E->SourceExpr) ? "}" : ")");
      return;
    case Kind::Cast:
      Out << "(";
      E->Value.PrimalType.print(Out, Policy);
      Out << ")";
      emitPrimal(E->Operands.front(), Out, LoopContext);
      return;
    case Kind::Unary:
      Out << UnaryOperator::getOpcodeStr(E->UnaryOpcode) << "(";
      emitPrimal(E->Operands.front(), Out, LoopContext);
      Out << ")";
      return;
    case Kind::Binary:
      Out << "(";
      emitPrimal(E->Operands[0], Out, LoopContext);
      Out << " " << BinaryOperator::getOpcodeStr(E->BinaryOpcode) << " ";
      emitPrimal(E->Operands[1], Out, LoopContext);
      Out << ")";
      return;
    case Kind::Conditional:
      Out << "(";
      emitPrimal(E->Operands[0], Out, LoopContext);
      Out << " ? ";
      emitPrimal(E->Operands[1], Out, LoopContext);
      Out << " : ";
      emitPrimal(E->Operands[2], Out, LoopContext);
      Out << ")";
      return;
    case Kind::Call:
      if (E->Receiver) {
        if (isa<CXXThisExpr>(E->Receiver->SourceExpr->IgnoreParenImpCasts())) {
          const auto *Method = cast<CXXMethodDecl>(E->Callee);
          Out << "::" << Method->getParent()->getName() << "::";
        } else if (hlsl::IsHLSLResourceCarrierType(
                       E->Receiver->Value.PrimalType)) {
          const auto *Method = cast<CXXMethodDecl>(E->Callee);
          Out << "((::" << Method->getParent()->getQualifiedNameAsString()
              << ")(";
          emitPrimal(E->Receiver, Out, LoopContext);
          Out << ")).";
        } else {
          emitPrimal(E->Receiver, Out, LoopContext);
          Out << ".";
        }
      } else if (const auto *Method = dyn_cast<CXXMethodDecl>(E->Callee))
        Out << "::" << Method->getParent()->getName() << "::";
      else {
        std::string QualifiedName = E->Callee->getQualifiedNameAsString();
        Out << "::"
            << StringRef(QualifiedName).drop_back(E->Callee->getName().size());
      }
      Out << E->Callee->getName() << "(";
      for (unsigned I = 0; I < E->Operands.size(); ++I) {
        if (I)
          Out << ", ";
        emitPrimal(E->Operands[I], Out, LoopContext);
      }
      Out << ")";
      return;
    default:
      E->SourceExpr->printPretty(Out, nullptr, Policy);
      return;
    }
  }

  std::string component(StringRef Cotangent, unsigned Index,
                        QualType Type) const {
    if (hlsl::IsHLSLMatType(Type)) {
      uint32_t Rows = 0;
      uint32_t Columns = 0;
      hlsl::GetHLSLMatRowColCount(Type, Rows, Columns);
      assert(Index < Rows * Columns && "matrix component index out of range");
      return (Cotangent + "[" + Twine(Index / Columns) + "][" +
              Twine(Index % Columns) + "]")
          .str();
    }
    if (hlsl::IsHLSLVecType(Type))
      return (Cotangent + "." + Twine(getComponentName(Index))).str();
    if (const auto *Vector =
            dyn_cast<VectorType>(Type.getCanonicalType().getTypePtr())) {
      assert(Index < Vector->getNumElements() &&
             "vector component index out of range");
      return (Cotangent + "." + Twine(getComponentName(Index))).str();
    }
    if (const auto *Record = Type->getAs<RecordType>()) {
      for (const FieldDecl *Field : Record->getDecl()->fields()) {
        unsigned FieldCount = getComponentCount(Field->getType());
        if (Index < FieldCount) {
          std::string FieldCotangent =
              (Cotangent + "." + Field->getName()).str();
          return component(FieldCotangent, Index, Field->getType());
        }
        Index -= FieldCount;
      }
      llvm_unreachable("record component index out of range");
    }
    if (const auto *Array =
            dyn_cast<ConstantArrayType>(Type.getCanonicalType().getTypePtr())) {
      unsigned ElementCount = getComponentCount(Array->getElementType());
      unsigned ArrayIndex = Index / ElementCount;
      assert(ArrayIndex < Array->getSize().getLimitedValue() &&
             "array component index out of range");
      std::string ElementCotangent =
          (Cotangent + "[" + Twine(ArrayIndex) + "]").str();
      return component(ElementCotangent, Index % ElementCount,
                       Array->getElementType());
    }
    unsigned Count = getComponentCount(Type);
    if (Count == 1)
      return Cotangent.str();
    return (Cotangent + "." + Twine(getComponentName(Index))).str();
  }

  std::string constructCotangent(QualType Type, ArrayRef<std::string> Values) {
    if (Values.size() == 1)
      return Values.front();
    std::string Text = printType(Type, Policy) + "(";
    for (unsigned I = 0; I < Values.size(); ++I) {
      if (I)
        Text += ", ";
      Text += Values[I];
    }
    Text += ")";
    return Text;
  }

  std::string adaptCotangent(StringRef Cotangent, QualType SourceType,
                             QualType TargetType) {
    unsigned SourceCount = getComponentCount(SourceType);
    unsigned TargetCount = getComponentCount(TargetType);
    if (SourceCount == TargetCount)
      return Cotangent.str();
    if (TargetCount == 1) {
      std::string Sum;
      std::string Parenthesized = "(" + Cotangent.str() + ")";
      for (unsigned I = 0; I < SourceCount; ++I) {
        std::string Term = component(Parenthesized, I, SourceType);
        Sum = Sum.empty() ? Term : "(" + Sum + " + " + Term + ")";
      }
      return Sum;
    }
    return Cotangent.str();
  }

  std::string zero(QualType Type) {
    return "(" + printType(Type, Policy) + ")0";
  }

  template <typename EmitAdjointFn, typename PrimalTextFn>
  void emitCallAdjoint(const hlsl::autodiff::ADExpr *E, StringRef Cotangent,
                       StringRef Indent, EmitAdjointFn EmitAdjoint,
                       PrimalTextFn PrimalText) {
    unsigned CallID = PullbackCallCount++;
    SmallVector<ActiveContextInfo, 4> CalleeContexts;
    SmallVector<unsigned, 4> ArgumentContexts(E->Operands.size(), 0);
    for (unsigned I = 0; I < E->Operands.size(); ++I) {
      const ParmVarDecl *Parameter = E->Callee->getParamDecl(I);
      if (isInactiveParameter(Parameter))
        continue;
      ActiveContextInfo *MatchingContext = nullptr;
      for (ActiveContextInfo &Context : CalleeContexts)
        if (E->Callee->getASTContext().hasSameType(Context.Type,
                                                   Parameter->getType())) {
          MatchingContext = &Context;
          break;
        }
      if (!MatchingContext) {
        CalleeContexts.push_back({Parameter->getType(), "", {}});
        MatchingContext = &CalleeContexts.back();
      }
      MatchingContext->Parameters.push_back(Parameter);
      ArgumentContexts[I] = MatchingContext - CalleeContexts.data();
    }

    for (unsigned I = 0; I < CalleeContexts.size(); ++I) {
      ActiveContextInfo &Context = CalleeContexts[I];
      Context.Name =
          "__dxc_ad_call_" + Twine(CallID).str() + "_context_" + Twine(I).str();
      std::string Type = printType(Context.Type, Policy);
        OS << Indent << "GradientContext<" << Type << "> " << Context.Name
         << " = (GradientContext<" << Type << ">)0;\n";
    }

    SmallVector<std::string, 4> Variables(E->Operands.size());
    for (unsigned I = 0; I < E->Operands.size(); ++I) {
      const ParmVarDecl *Parameter = E->Callee->getParamDecl(I);
      if (isInactiveParameter(Parameter))
        continue;
      Variables[I] =
          "__dxc_ad_call_" + Twine(CallID).str() + "_arg_" + Twine(I).str();
      std::string Type = printType(Parameter->getType(), Policy);
      OS << Indent << "Variable<" << Type << "> " << Variables[I]
         << " = variable(" << CalleeContexts[ArgumentContexts[I]].Name << ", "
         << PrimalText(E->Operands[I]) << ");\n";
    }

    std::string ReceiverVariable;
    if (E->Receiver &&
        !isa<CXXThisExpr>(
            E->Receiver->SourceExpr->IgnoreParenImpCasts()) &&
        hlsl::IsHLSLResourceCarrierType(E->Receiver->Value.PrimalType)) {
      const auto *Method = cast<CXXMethodDecl>(E->Callee);
      const CXXRecordDecl *Record = Method->getParent();
      ReceiverVariable =
          "__dxc_ad_call_" + Twine(CallID).str() + "_receiver";
      OS << Indent << "::user::ad::bwd::"
         << Record->getQualifiedNameAsString() << " " << ReceiverVariable
         << ";\n";
      for (const FieldDecl *Field : Record->fields())
        if (Field->getIdentifier())
          OS << Indent << ReceiverVariable << "." << Field->getName() << " = ("
             << PrimalText(E->Receiver) << ")." << Field->getName() << ";\n";
    }

    OS << Indent;
    if (E->Receiver) {
      if (isa<CXXThisExpr>(
              E->Receiver->SourceExpr->IgnoreParenImpCasts())) {
        OS << "this.";
      } else if (!ReceiverVariable.empty()) {
        OS << ReceiverVariable << ".";
      } else {
        const auto *Method = cast<CXXMethodDecl>(E->Callee);
        OS << "((::user::ad::bwd::"
            << Method->getParent()->getQualifiedNameAsString() << ")(" 
           << PrimalText(E->Receiver) << ")).";
      }
    } else if (const auto *Method = dyn_cast<CXXMethodDecl>(E->Callee)) {
      OS << "::user::ad::bwd::"
         << Method->getParent()->getQualifiedNameAsString() << "::";
    } else {
      SmallVector<StringRef, 4> ReversedNamespaces;
      for (const DeclContext *DC = E->Callee->getDeclContext(); DC;
           DC = DC->getParent()) {
        const auto *Namespace = dyn_cast<NamespaceDecl>(DC);
        if (Namespace && Namespace->getIdentifier())
          ReversedNamespaces.push_back(Namespace->getName());
      }
      OS << "::user::ad::bwd::";
      for (auto It = ReversedNamespaces.rbegin();
           It != ReversedNamespaces.rend(); ++It)
        OS << *It << "::";
    }
    OS << E->Callee->getName() << "(";
    bool First = true;
    for (const ActiveContextInfo &Context : CalleeContexts) {
      if (!First)
        OS << ", ";
      First = false;
      OS << Context.Name;
    }
    for (unsigned I = 0; I < E->Operands.size(); ++I) {
      if (!First)
        OS << ", ";
      First = false;
      if (isInactiveParameter(E->Callee->getParamDecl(I)))
        OS << PrimalText(E->Operands[I]);
      else
        OS << Variables[I];
    }
    if (!First)
      OS << ", ";
    OS << Cotangent << ");\n";

    for (unsigned I = 0; I < E->Operands.size(); ++I) {
      if (isInactiveParameter(E->Callee->getParamDecl(I)))
        continue;
      EmitAdjoint(E->Operands[I], Variables[I] + ".gradient(" +
                                      CalleeContexts[ArgumentContexts[I]].Name +
                                      ")");
    }
  }

  template <typename EmitAdjointFn, typename PrimalTextFn>
  void emitCustomCallAdjoint(const hlsl::autodiff::ADExpr *E,
                             StringRef Cotangent, StringRef Indent,
                             EmitAdjointFn EmitAdjoint,
                             PrimalTextFn PrimalText) {
    const FunctionDecl *Derivative =
        hlsl::autodiff::getADBackwardDerivative(E->Callee);
    assert(Derivative && "custom pullback call has no associated derivative");
    unsigned CallID = PullbackCallCount++;
    SmallVector<std::string, 4> Cotangents(E->Operands.size());
    for (unsigned I = 0; I < E->Operands.size(); ++I) {
      if (isInactiveParameter(E->Callee->getParamDecl(I)) ||
          E->Operands[I]->Value.Activity ==
              hlsl::autodiff::ADActivity::Inactive)
        continue;
      Cotangents[I] = "__dxc_ad_custom_call_" + Twine(CallID).str() +
                      "_arg_" + Twine(I).str() + "_adjoint";
      OS << Indent << printType(E->Operands[I]->Value.PrimalType, Policy) << " "
         << Cotangents[I] << " = "
         << zero(E->Operands[I]->Value.PrimalType) << ";\n";
    }

    OS << Indent;
    if (E->Receiver) {
      OS << PrimalText(E->Receiver) << "." << Derivative->getName();
    } else if (const auto *Method = dyn_cast<CXXMethodDecl>(Derivative)) {
      OS << "::" << Method->getParent()->getName() << "::"
         << Derivative->getName();
    } else {
      OS << "::" << Derivative->getQualifiedNameAsString();
    }
    OS << "(";
    bool First = true;
    for (const hlsl::autodiff::ADExpr *Operand : E->Operands) {
      if (!First)
        OS << ", ";
      First = false;
      OS << PrimalText(Operand);
    }
    if (!First)
      OS << ", ";
    OS << Cotangent;
    for (const std::string &ArgumentCotangent : Cotangents)
      if (!ArgumentCotangent.empty())
        OS << ", " << ArgumentCotangent;
    OS << ");\n";

    for (unsigned I = 0; I < E->Operands.size(); ++I)
      if (!Cotangents[I].empty())
        EmitAdjoint(E->Operands[I], Cotangents[I]);
  }

  template <typename EmitAdjointFn, typename PrimalTextFn>
  bool emitTypedPullback(const hlsl::autodiff::ADExpr *E, StringRef Cotangent,
                         EmitAdjointFn EmitAdjoint,
                         PrimalTextFn PrimalText) {
    using Activity = hlsl::autodiff::ADActivity;
    using Kind = hlsl::autodiff::ADExpr::Kind;
    using Rule = hlsl::autodiff::ADPullbackRule;
    Rule PullbackRule = hlsl::autodiff::getADPullbackRule(E).Rule;
    switch (PullbackRule) {
    case Rule::Swizzle: {
      const hlsl::autodiff::ADExpr *Base = E->Operands.front();
      unsigned BaseCount = getComponentCount(Base->Value.PrimalType);
      SmallVector<std::string, 4> Values;
      for (unsigned BaseIndex = 0; BaseIndex < BaseCount; ++BaseIndex) {
        std::string Sum;
        for (unsigned I = 0; I < E->Components.size(); ++I) {
          if (E->Components[I] != BaseIndex)
            continue;
          std::string Term = component(Cotangent, I, E->Value.PrimalType);
          Sum = Sum.empty() ? Term : "(" + Sum + " + " + Term + ")";
        }
        Values.push_back(Sum.empty() ? "0.0f" : Sum);
      }
      EmitAdjoint(Base, constructCotangent(Base->Value.PrimalType, Values));
      return true;
    }
    case Rule::Subscript: {
      const hlsl::autodiff::ADExpr *Base = E->Operands[0];
      std::string Index = PrimalText(E->Operands[1]);
      SmallVector<std::string, 4> Values;
      if (hlsl::IsHLSLMatType(Base->Value.PrimalType)) {
        uint32_t Rows = 0;
        uint32_t Columns = 0;
        hlsl::GetHLSLMatRowColCount(Base->Value.PrimalType, Rows, Columns);
        for (unsigned Row = 0; Row < Rows; ++Row)
          for (unsigned Column = 0; Column < Columns; ++Column)
            Values.push_back("(" + Index + " == " + Twine(Row).str() +
                             " ? " +
                             component(Cotangent, Column,
                                       E->Value.PrimalType) +
                             " : 0.0f)");
      } else {
        unsigned BaseCount = getComponentCount(Base->Value.PrimalType);
        for (unsigned I = 0; I < BaseCount; ++I)
          Values.push_back("(" + Index + " == " + Twine(I).str() + " ? " +
                           Cotangent.str() + " : 0.0f)");
      }
      EmitAdjoint(Base, constructCotangent(Base->Value.PrimalType, Values));
      return true;
    }
    case Rule::AggregateConstruct: {
      unsigned Offset = 0;
      auto EmitAggregate = [&](const auto &Self,
                               const hlsl::autodiff::ADExpr *Aggregate) -> void {
        for (const hlsl::autodiff::ADExpr *Operand : Aggregate->Operands) {
          unsigned OperandCount = getComponentCount(Operand->Value.PrimalType);
          if (Operand->Value.Activity == Activity::Inactive) {
            Offset += OperandCount;
            continue;
          }
          if (Operand->K == Kind::AggregateConstruct) {
            Self(Self, Operand);
            continue;
          }
          SmallVector<std::string, 4> Values;
          for (unsigned I = 0; I < OperandCount; ++I)
            Values.push_back(
                component(Cotangent, Offset + I, E->Value.PrimalType));
          EmitAdjoint(
              Operand, constructCotangent(Operand->Value.PrimalType, Values));
          Offset += OperandCount;
        }
      };
      EmitAggregate(EmitAggregate, E);
      return true;
    }
    case Rule::Cast:
      EmitAdjoint(E->Operands.front(),
                  "(" +
                      printType(E->Operands.front()->Value.PrimalType, Policy) +
                      ")(" + Cotangent.str() + ")");
      return true;
    case Rule::Positive:
      EmitAdjoint(E->Operands.front(), Cotangent.str());
      return true;
    case Rule::Negative:
      EmitAdjoint(E->Operands.front(), "-(" + Cotangent.str() + ")");
      return true;
    case Rule::Add:
      EmitAdjoint(E->Operands[0],
                  adaptCotangent(Cotangent, E->Value.PrimalType,
                                  E->Operands[0]->Value.PrimalType));
      EmitAdjoint(E->Operands[1],
                  adaptCotangent(Cotangent, E->Value.PrimalType,
                                  E->Operands[1]->Value.PrimalType));
      return true;
    case Rule::Subtract:
      EmitAdjoint(E->Operands[0],
                  adaptCotangent(Cotangent, E->Value.PrimalType,
                                  E->Operands[0]->Value.PrimalType));
      EmitAdjoint(E->Operands[1],
                  adaptCotangent("-(" + Cotangent.str() + ")",
                                  E->Value.PrimalType,
                                  E->Operands[1]->Value.PrimalType));
      return true;
    case Rule::Multiply: {
      std::string Left = "(" + Cotangent.str() + " * " +
                         PrimalText(E->Operands[1]) + ")";
      std::string Right = "(" + Cotangent.str() + " * " +
                          PrimalText(E->Operands[0]) + ")";
      EmitAdjoint(E->Operands[0],
                  adaptCotangent(Left, E->Value.PrimalType,
                                  E->Operands[0]->Value.PrimalType));
      EmitAdjoint(E->Operands[1],
                  adaptCotangent(Right, E->Value.PrimalType,
                                  E->Operands[1]->Value.PrimalType));
      return true;
    }
    case Rule::Divide: {
      std::string Numerator = PrimalText(E->Operands[0]);
      std::string Denominator = PrimalText(E->Operands[1]);
      EmitAdjoint(E->Operands[0],
                  adaptCotangent("(" + Cotangent.str() + " / " +
                                      Denominator + ")",
                                  E->Value.PrimalType,
                                  E->Operands[0]->Value.PrimalType));
      EmitAdjoint(E->Operands[1],
                  adaptCotangent("(-(" + Cotangent.str() + ") * " +
                                      Numerator + " / (" + Denominator +
                                      " * " + Denominator + "))",
                                  E->Value.PrimalType,
                                  E->Operands[1]->Value.PrimalType));
      return true;
    }
    case Rule::Conditional: {
      std::string Condition = PrimalText(E->Operands[0]);
      EmitAdjoint(E->Operands[1],
                  "(" + Condition + " ? " + Cotangent.str() + " : " +
                      zero(E->Operands[1]->Value.PrimalType) + ")");
      EmitAdjoint(E->Operands[2],
                  "(" + Condition + " ? " +
                      zero(E->Operands[2]->Value.PrimalType) + " : " +
                      Cotangent.str() + ")");
      return true;
    }
    case Rule::Sin:
      EmitAdjoint(E->Operands.front(),
                  "(" + Cotangent.str() + " * cos(" +
                      PrimalText(E->Operands.front()) + "))");
      return true;
    case Rule::Cos:
      EmitAdjoint(E->Operands.front(),
                  "-(" + Cotangent.str() + " * sin(" +
                      PrimalText(E->Operands.front()) + "))");
      return true;
    case Rule::Exp:
      EmitAdjoint(E->Operands.front(),
                  "(" + Cotangent.str() + " * exp(" +
                      PrimalText(E->Operands.front()) + "))");
      return true;
    case Rule::Log:
      EmitAdjoint(E->Operands.front(),
                  "(" + Cotangent.str() + " / " +
                      PrimalText(E->Operands.front()) + ")");
      return true;
    case Rule::Sqrt:
      EmitAdjoint(E->Operands.front(),
                  "(" + Cotangent.str() + " * (0.5f / sqrt(" +
                      PrimalText(E->Operands.front()) + ")))");
      return true;
    case Rule::Unsupported:
    case Rule::Leaf:
    case Rule::ComposedCall:
    case Rule::CustomCall:
      return false;
    }
    llvm_unreachable("unknown typed pullback rule");
  }

  std::string genericLoopPrimalText(const hlsl::autodiff::ADExpr *E,
                                    const hlsl::autodiff::ADLoopPlan &Loop,
                                    LoopPrimalContext::Mode Mode =
                                        LoopPrimalContext::Mode::StaticTape) {
    std::string Text;
    raw_string_ostream Stream(Text);
    LoopPrimalContext Context{&Loop, Mode};
    emitPrimal(E, Stream, &Context);
    Stream.flush();
    return Text;
  }

  void emitGenericLoopPullback(const hlsl::autodiff::ADExpr *E,
                               StringRef Cotangent,
                               ArrayRef<std::string> StateAdjoints,
                   const hlsl::autodiff::ADLoopPlan &Loop,
                   LoopPrimalContext::Mode PrimalMode =
                     LoopPrimalContext::Mode::StaticTape) {
    using Activity = hlsl::autodiff::ADActivity;
    using Kind = hlsl::autodiff::ADExpr::Kind;
    if (E->Value.Activity == Activity::Inactive)
      return;
    if (E->K == Kind::LoopStateRef) {
      OS << "        " << StateAdjoints[E->LoopStateIndex]
         << " += " << Cotangent << ";\n";
      return;
    }
    auto EmitAdjoint = [&](const hlsl::autodiff::ADExpr *Operand,
                           const std::string &OperandCotangent) {
      emitGenericLoopPullback(Operand, OperandCotangent, StateAdjoints, Loop,
                              PrimalMode);
    };
    auto PrimalText = [&](const hlsl::autodiff::ADExpr *Operand) {
      return genericLoopPrimalText(Operand, Loop, PrimalMode);
    };
    hlsl::autodiff::ADPullbackRule Rule =
        hlsl::autodiff::getADPullbackRule(E).Rule;
    if (Rule == hlsl::autodiff::ADPullbackRule::ComposedCall) {
      emitCallAdjoint(E, Cotangent, "        ", EmitAdjoint, PrimalText);
      return;
    }
    if (Rule == hlsl::autodiff::ADPullbackRule::CustomCall) {
      emitCustomCallAdjoint(E, Cotangent, "        ", EmitAdjoint, PrimalText);
      return;
    }
    if (emitTypedPullback(E, Cotangent, EmitAdjoint, PrimalText))
      return;
    llvm_unreachable("validated generic loop pullback node");
  }

  void emitAdjoint(const hlsl::autodiff::ADExpr *E, StringRef Cotangent) {
    using Activity = hlsl::autodiff::ADActivity;
    using Kind = hlsl::autodiff::ADExpr::Kind;
    if (E->Value.Activity == Activity::Inactive)
      return;
    switch (E->K) {
    case Kind::DeclRef:
      OS << "    " << contextName(E->SourceDecl) << ".gradients["
         << E->SourceDecl->getName() << ".id] += " << Cotangent << ";\n";
      return;
    case Kind::LocalRef:
      emitAdjoint(E->Binding->Value, Cotangent);
      return;
    case Kind::Call:
      if (hlsl::autodiff::getADPullbackRule(E).Rule ==
          hlsl::autodiff::ADPullbackRule::ComposedCall) {
        emitCallAdjoint(
            E, Cotangent, "    ",
            [&](const hlsl::autodiff::ADExpr *Operand,
                const std::string &OperandCotangent) {
              emitAdjoint(Operand, OperandCotangent);
            },
            [&](const hlsl::autodiff::ADExpr *Operand) {
              return primalText(Operand);
            });
        return;
      }
      if (hlsl::autodiff::getADPullbackRule(E).Rule ==
          hlsl::autodiff::ADPullbackRule::CustomCall) {
        emitCustomCallAdjoint(
            E, Cotangent, "    ",
            [&](const hlsl::autodiff::ADExpr *Operand,
                const std::string &OperandCotangent) {
              emitAdjoint(Operand, OperandCotangent);
            },
            [&](const hlsl::autodiff::ADExpr *Operand) {
              return primalText(Operand);
            });
        return;
      }
      break;
    case Kind::RuntimeLoopResult: {
      unsigned LoopID = RuntimeLoopIDs.lookup(E);
      std::string Type = printType(E->Value.PrimalType, Policy);
      auto LoopIt = RuntimeLoopPlans.find(E);
      if (LoopIt != RuntimeLoopPlans.end()) {
        const hlsl::autodiff::ADLoopPlan &Loop = *LoopIt->second;
        SmallVector<std::string, 4> Adjoints;
        for (unsigned I = 0; I < Loop.States.size(); ++I) {
          std::string Adjoint = "__dxc_ad_loop_" + Twine(LoopID).str() +
                                "_state_" + Twine(I).str() + "_adjoint";
          Adjoints.push_back(Adjoint);
          OS << "    " << Type << " " << Adjoint << " = "
             << (Loop.States[I].Result == E ? Cotangent.str()
                                            : zero(E->Value.PrimalType))
             << ";\n";
        }
        OS << "    for (uint " << Loop.Counter->getName() << " = ";
        emitPrimal(Loop.TripCount);
        OS << "; " << Loop.Counter->getName() << " > 0;) {\n"
           << "        --" << Loop.Counter->getName() << ";\n";
        if (Loop.Storage == hlsl::autodiff::ADLoopStorageKind::Recompute) {
          for (const hlsl::autodiff::ADLoopState &State : Loop.States) {
            unsigned StateLoopID = RuntimeLoopIDs.lookup(State.Result);
            OS << "        " << printType(State.PrimalType, Policy)
               << " __dxc_ad_loop_" << StateLoopID << "_replay = "
               << "__dxc_ad_loop_" << StateLoopID << "_initial;\n";
          }
          std::string ReplayCounter =
              "__dxc_ad_loop_" + Twine(LoopID).str() + "_replay_index";
          OS << "        for (uint " << ReplayCounter << " = 0; "
             << ReplayCounter << " < " << Loop.Counter->getName() << "; ++"
             << ReplayCounter << ") {\n";
          for (const hlsl::autodiff::ADLoopUpdate &Update : Loop.Updates) {
            const hlsl::autodiff::ADLoopState &State =
                Loop.States[Update.TargetStateIndex];
            unsigned StateLoopID = RuntimeLoopIDs.lookup(State.Result);
            OS << "            __dxc_ad_loop_" << StateLoopID << "_replay ";
            switch (Update.Opcode) {
            case BO_Assign:
              OS << "= ";
              break;
            case BO_Add:
              OS << "+= ";
              break;
            case BO_Sub:
              OS << "-= ";
              break;
            case BO_Mul:
              OS << "*= ";
              break;
            case BO_Div:
              OS << "/= ";
              break;
            default:
              llvm_unreachable("validated active runtime loop operation");
            }
            OS << genericLoopPrimalText(
                      Update.Value, Loop,
                      LoopPrimalContext::Mode::ReplayState)
               << ";\n";
          }
          OS << "        }\n";
          for (const hlsl::autodiff::ADLoopState &State : Loop.States) {
            unsigned StateLoopID = RuntimeLoopIDs.lookup(State.Result);
            OS << "        " << printType(State.PrimalType, Policy)
               << " __dxc_ad_loop_" << StateLoopID
               << "_version_0_replay = __dxc_ad_loop_" << StateLoopID
               << "_replay;\n";
          }
          for (const hlsl::autodiff::ADLoopUpdate &Update : Loop.Updates) {
            const hlsl::autodiff::ADLoopState &State =
                Loop.States[Update.TargetStateIndex];
            unsigned StateLoopID = RuntimeLoopIDs.lookup(State.Result);
            OS << "        " << printType(State.PrimalType, Policy)
               << " __dxc_ad_loop_" << StateLoopID << "_version_"
               << Update.ResultVersion << "_replay = "
               << genericLoopPrimalText(
                      Update.Pullback.Value, Loop,
                      LoopPrimalContext::Mode::ReplayVersion)
               << ";\n";
          }
        }
        for (unsigned I = Loop.Updates.size(); I > 0; --I) {
          const hlsl::autodiff::ADLoopUpdate &Update = Loop.Updates[I - 1];
          StringRef TargetAdjoint = Adjoints[Update.TargetStateIndex];
          std::string Incoming = "__dxc_ad_loop_" + Twine(LoopID).str() +
                                 "_update_" + Twine(I - 1).str() + "_adjoint";
          OS << "        " << Type << " " << Incoming << " = " << TargetAdjoint
             << ";\n"
             << "        " << TargetAdjoint << " = "
             << zero(E->Value.PrimalType) << ";\n";
            emitGenericLoopPullback(
              Update.Pullback.Value, Incoming, Adjoints, Loop,
              Loop.Storage == hlsl::autodiff::ADLoopStorageKind::Recompute
                ? LoopPrimalContext::Mode::ReplayVersion
                : LoopPrimalContext::Mode::StaticTape);
        }
        OS << "    }\n";
        for (unsigned I = 0; I < Loop.States.size(); ++I)
          emitAdjoint(Loop.States[I].InitialValue, Adjoints[I]);
        return;
      }
      llvm_unreachable("validated runtime loop has no generic pullback");
    }
    default:
      break;
    }
    auto EmitAdjoint = [&](const hlsl::autodiff::ADExpr *Operand,
                           const std::string &OperandCotangent) {
      emitAdjoint(Operand, OperandCotangent);
    };
    auto PrimalText = [&](const hlsl::autodiff::ADExpr *Operand) {
      return primalText(Operand);
    };
    (void)emitTypedPullback(E, Cotangent, EmitAdjoint, PrimalText);
  }
};

// Render the autodiff signature for a function in either mode.
void emitAutoDiffSignature(const FunctionDecl *FD, AutoDiffEmitter::Mode M,
                           StringRef ElemType,
                           ArrayRef<ActiveContextInfo> Contexts,
                           const PrintingPolicy &Policy, raw_ostream &OS) {
  if (const auto *Method = dyn_cast<CXXMethodDecl>(FD))
    if (Method->isStatic())
      OS << "static ";
  if (M == AutoDiffEmitter::Fwd) {
    OS << "Value<" << ElemType << "> " << FD->getName() << "(";
    bool First = true;
    for (const ParmVarDecl *P : FD->parameters()) {
      if (!First)
        OS << ", ";
      First = false;
      std::string ParamType = printAutoDiffParameterType(FD, P, Policy);
      if (isInactiveParameter(P))
        OS << ParamType;
      else
        OS << "Value<" << ParamType << ">";
      OS << " " << P->getName();
    }
    OS << ")";
    return;
  }
  // Backward mode returns the primal value; derivatives are written to the
  // GradientContext entries associated with the Variable<T> parameters.
  OS << ElemType << " " << FD->getName() << "(";
  bool First = true;
  for (const ActiveContextInfo &Context : Contexts) {
    if (!First)
      OS << ", ";
    First = false;
    OS << "inout GradientContext<" << printType(Context.Type, Policy) << "> "
       << Context.Name;
  }
  for (const ParmVarDecl *P : FD->parameters()) {
    std::string ParamType = printAutoDiffParameterType(FD, P, Policy);
    if (!First)
      OS << ", ";
    First = false;
    if (isInactiveParameter(P))
      OS << ParamType;
    else
      OS << "Variable<" << ParamType << ">";
    OS << " " << P->getName();
  }
  if (!First)
    OS << ", ";
  OS << ElemType << " __dxc_ad_seed)";
}

SmallVector<ActiveContextInfo, 4> buildActiveContexts(const FunctionDecl *FD) {
  SmallVector<ActiveContextInfo, 4> Contexts;
  for (const ParmVarDecl *P : FD->parameters()) {
    if (isInactiveParameter(P))
      continue;
    ActiveContextInfo *MatchingContext = nullptr;
    for (ActiveContextInfo &Context : Contexts)
      if (FD->getASTContext().hasSameType(Context.Type, P->getType())) {
        MatchingContext = &Context;
        break;
      }
    if (!MatchingContext) {
      Contexts.push_back({P->getType(), "", {}});
      MatchingContext = &Contexts.back();
    }
    MatchingContext->Parameters.push_back(P);
  }
  if (Contexts.empty())
    Contexts.push_back({FD->getReturnType(), "context", {}});
  else if (Contexts.size() == 1)
    Contexts.front().Name = "context";
  else
    for (ActiveContextInfo &Context : Contexts)
      Context.Name = Context.Parameters.front()->getName().str() + "_context";
  return Contexts;
}

bool hasBackwardCallCycle(const FunctionDecl *FD,
                          SmallPtrSetImpl<const FunctionDecl *> &Active,
                          SmallPtrSetImpl<const FunctionDecl *> &Complete);

bool statementReachesBackwardCallCycle(
    const Stmt *S, SmallPtrSetImpl<const FunctionDecl *> &Active,
    SmallPtrSetImpl<const FunctionDecl *> &Complete) {
  if (!S)
    return false;
  if (const auto *Call = dyn_cast<CallExpr>(S)) {
    const FunctionDecl *Callee = Call->getDirectCallee();
    if (const FunctionDecl *Substitute =
            hlsl::autodiff::getADPrimalSubstitute(Callee))
      Callee = Substitute;
    const HLSLAutoDiffAttr *Attr = getAutoDiffAttr(Callee);
    if (Callee && Attr && Attr->hasBackward() &&
        hasBackwardCallCycle(Callee, Active, Complete))
      return true;
  }
  for (const Stmt *Child : S->children())
    if (statementReachesBackwardCallCycle(Child, Active, Complete))
      return true;
  return false;
}

bool hasBackwardCallCycle(const FunctionDecl *FD,
                          SmallPtrSetImpl<const FunctionDecl *> &Active,
                          SmallPtrSetImpl<const FunctionDecl *> &Complete) {
  FD = FD->getCanonicalDecl();
  if (Active.count(FD))
    return true;
  if (Complete.count(FD))
    return false;

  Active.insert(FD);
  const FunctionDecl *Definition = nullptr;
  bool HasCycle = FD->hasBody(Definition) &&
                  statementReachesBackwardCallCycle(Definition->getBody(),
                                                    Active, Complete);
  Active.erase(FD);
  Complete.insert(FD);
  return HasCycle;
}

bool hasBackwardCallCycle(const FunctionDecl *FD) {
  SmallPtrSet<const FunctionDecl *, 8> Active;
  SmallPtrSet<const FunctionDecl *, 8> Complete;
  return hasBackwardCallCycle(FD, Active, Complete);
}

// Emit the auto-diff variant of a single function inside the appropriate
// namespace block. Returns true if anything was written.
bool emitAutoDiffFunction(const FunctionDecl *FD, AutoDiffEmitter::Mode M,
                          const PrintingPolicy &Policy, raw_ostream &OS) {
  // Determine the element type from the return type. We support scalar
  // float-like functions for now; other return types produce a TODO.
  std::string ElemType = printType(FD->getReturnType(), Policy);
  SmallVector<ActiveContextInfo, 4> Contexts = buildActiveContexts(FD);

  QualType ActiveType = Contexts.front().Type;
  bool NeedsDirectReverse =
      M == AutoDiffEmitter::Bwd &&
      (Contexts.size() > 1 ||
       !FD->getASTContext().hasSameType(ActiveType, FD->getReturnType()));

  // Render the body into a temporary buffer first so that, if a
  // non-differentiable construct was encountered, we can discard the body
  // and emit a _Static_assert stub instead. This makes the generated
  // header self-diagnosing: instantiating the function fails at compile
  // time with a clear message.
  std::string BodyText;
  raw_string_ostream BodyOS(BodyText);
  std::string Reason;
  bool ValidBody = false;

  if (const auto *CS = dyn_cast_or_null<CompoundStmt>(FD->getBody())) {
    hlsl::autodiff::ADFunctionPlan Plan;
    std::string PlanReason;
    bool HasTypedPlan =
        hlsl::autodiff::BuildADFunctionPlan(FD, Plan, PlanReason);
    if (M == AutoDiffEmitter::Bwd && HasTypedPlan)
      for (const std::unique_ptr<hlsl::autodiff::ADExpr> &Expr :
           Plan.Expressions)
        if (Expr->Value.Activity == hlsl::autodiff::ADActivity::Active &&
            (Expr->K == hlsl::autodiff::ADExpr::Kind::Cast ||
             (Expr->K == hlsl::autodiff::ADExpr::Kind::Call && Expr->Callee &&
              ((getAutoDiffAttr(Expr->Callee) &&
                getAutoDiffAttr(Expr->Callee)->hasBackward()) ||
               hlsl::autodiff::getADBackwardDerivative(Expr->Callee))))) {
          NeedsDirectReverse = true;
          break;
        }
    if (M == AutoDiffEmitter::Bwd && HasTypedPlan)
      for (const hlsl::autodiff::ADStmt &Statement : Plan.Statements)
        if (Statement.K == hlsl::autodiff::ADStmt::Kind::ActiveLoop) {
          NeedsDirectReverse = true;
          break;
        }
    bool HasTerminalPlanFailure =
        !HasTypedPlan &&
        (StringRef(PlanReason).startswith("active ") ||
         StringRef(PlanReason).startswith("nonlinear active runtime loop "));
    if (M == AutoDiffEmitter::Bwd && HasTypedPlan)
      for (const std::unique_ptr<hlsl::autodiff::ADExpr> &Expr :
         Plan.Expressions)
      if (Expr->K == hlsl::autodiff::ADExpr::Kind::Call && Expr->Receiver &&
        Expr->Value.Activity == hlsl::autodiff::ADActivity::Active &&
        hlsl::IsHLSLResourceType(Expr->Receiver->Value.PrimalType) &&
        hlsl::autodiff::getADPullbackRule(Expr.get()).Rule ==
          hlsl::autodiff::ADPullbackRule::Unsupported) {
        HasTypedPlan = false;
        HasTerminalPlanFailure = true;
        PlanReason = "resource operation '" + Expr->Callee->getName().str() +
               "' requires an associated custom backward derivative";
        break;
      }
    if (M == AutoDiffEmitter::Bwd && HasTypedPlan && hasBackwardCallCycle(FD)) {
      HasTypedPlan = false;
      HasTerminalPlanFailure = true;
      PlanReason = "recursive pullback composition is not supported";
    }
    if (M == AutoDiffEmitter::Bwd) {
      for (const ParmVarDecl *P : FD->parameters()) {
        if (isInactiveParameter(P))
          continue;
        std::string ParamType = printType(P->getType(), Policy);
        BodyOS << "    VariableExpr<" << ParamType << "> " << P->getName()
               << "_expr = makeVariableExpr<" << ParamType << ">("
               << P->getName() << ");\n";
      }
    }
    if (HasTypedPlan && NeedsDirectReverse) {
      DirectReverseEmitter Em(ElemType, BodyOS, Policy, Contexts);
      ValidBody = Em.emitPlan(Plan);
      BodyOS.flush();
      if (!ValidBody)
        Reason = "cross-shape expression is not supported by direct reverse "
                 "lowering";
    } else if (HasTypedPlan) {
      TypedAutoDiffEmitter Em(M, ElemType, BodyOS, Policy);
      Em.emitPlan(Plan);
      BodyOS.flush();
      ValidBody = !Em.sawNonDifferentiable();
      if (!ValidBody)
        Reason = Em.nonDifferentiableReason().str();
    } else if (HasTerminalPlanFailure) {
      ValidBody = false;
      Reason = PlanReason;
    } else {
      AutoDiffEmitter Em(M, ElemType, BodyOS, Policy, FD->getASTContext());
      for (const Stmt *S : CS->body())
        Em.emitStmt(S, "    ");
      BodyOS.flush();
      ValidBody = !Em.sawNonDifferentiable();
      if (!ValidBody)
        Reason = Em.nonDifferentiableReason().str();
    }
  } else {
    ValidBody = false;
    Reason = "function has no body";
  }

  emitAutoDiffSignature(FD, M, ElemType, Contexts, Policy, OS);
  OS << " {\n";
  if (ValidBody) {
    OS << BodyText;
  } else {
    // Stub: instantiating the generated function is a compile-time error.
    // Quote the reason for the diagnostic and provide a concrete return so
    // the surrounding code still parses.
    OS << "    _Static_assert(false, \"auto-diff cannot generate "
       << (M == AutoDiffEmitter::Fwd ? "forward" : "backward") << "-mode for '"
       << FD->getName() << "': " << Reason << "\");\n";
    if (M == AutoDiffEmitter::Fwd)
      OS << "    return Value<" << ElemType << ">();\n";
    else
      OS << "    return (" << ElemType << ")0;\n";
  }
  OS << "}\n";
  return true;
}

// Walk every nested NamespaceDecl in \p DC whose name matches the next
// element of \p Path; when the path is exhausted, record the name of every
// FunctionDecl directly declared in that namespace into \p Names.
//
// Namespaces in C++ may be reopened, so the matching is done across all
// declarations in \p DC, not just the first match. The walk descends into
// nested NamespaceDecls only — it deliberately ignores other declaration
// contexts (linkage specs, records, etc.) because the user::ad::{fwd,bwd}
// names are required to be namespaces by the autodiff convention.
void collectFunctionNamesInNamespace(const DeclContext *DC,
                                     ArrayRef<StringRef> Path,
                                     StringSet<> &Names) {
  if (Path.empty()) {
    for (const Decl *D : DC->decls()) {
      if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
        if (FD->getIdentifier())
          Names.insert(FD->getName());
      }
    }
    return;
  }
  StringRef Head = Path.front();
  ArrayRef<StringRef> Tail = Path.slice(1);
  for (const Decl *D : DC->decls()) {
    const auto *NS = dyn_cast<NamespaceDecl>(D);
    if (!NS || !NS->getIdentifier() || NS->getName() != Head)
      continue;
    collectFunctionNamesInNamespace(NS, Tail, Names);
  }
}

// Locate the user-supplied wrapper record (`user::ad::fwd::ClassName` or
// `user::ad::bwd::ClassName`) for a given source class. Returns the first
// CXXRecordDecl found at the requested namespace path with the matching
// name, or nullptr if the user has not provided one.
//
// The lookup descends through possibly-reopened namespace chains; the
// match is by unqualified name only (mirroring collectUserAdFunctionNames).
const CXXRecordDecl *findUserAdRecord(const DeclContext *DC,
                                      ArrayRef<StringRef> Path,
                                      StringRef ClassName) {
  if (Path.empty()) {
    for (const Decl *D : DC->decls()) {
      const auto *RD = dyn_cast<CXXRecordDecl>(D);
      if (!RD || !RD->getIdentifier())
        continue;
      if (RD->getName() == ClassName)
        return RD;
    }
    return nullptr;
  }
  StringRef Head = Path.front();
  ArrayRef<StringRef> Tail = Path.slice(1);
  for (const Decl *D : DC->decls()) {
    const auto *NS = dyn_cast<NamespaceDecl>(D);
    if (!NS || !NS->getIdentifier() || NS->getName() != Head)
      continue;
    if (const CXXRecordDecl *Found = findUserAdRecord(NS, Tail, ClassName))
      return Found;
  }
  return nullptr;
}

// Populate \p FwdNames and \p BwdNames with the unqualified names of every
// function the user has already declared (or defined) inside
// `user::ad::fwd` and `user::ad::bwd` respectively. These names are used to
// suppress regeneration of the corresponding autodiff stubs, allowing users
// to provide hand-written differentials or to incrementally check the
// generated ones into source control.
void collectUserAdFunctionNames(const TranslationUnitDecl *TU,
                                StringSet<> &FwdNames, StringSet<> &BwdNames) {
  static const StringRef FwdPath[] = {"user", "ad", "fwd"};
  static const StringRef BwdPath[] = {"user", "ad", "bwd"};
  collectFunctionNamesInNamespace(TU, FwdPath, FwdNames);
  collectFunctionNamesInNamespace(TU, BwdPath, BwdNames);
}

class DirectAutoDiffCalleeCollector
    : public RecursiveASTVisitor<DirectAutoDiffCalleeCollector> {
public:
  bool VisitCallExpr(CallExpr *Call) {
    const FunctionDecl *Callee = Call->getDirectCallee();
    if (!Callee)
      return true;
    Callee = Callee->getCanonicalDecl();
    if (const FunctionDecl *Substitute =
            hlsl::autodiff::getADPrimalSubstitute(Callee))
      Callee = Substitute;
    if (getAutoDiffAttr(Callee) && Seen.insert(Callee).second)
      Callees.push_back(Callee);
    return true;
  }

  SmallVector<const FunctionDecl *, 4> Callees;

private:
  SmallPtrSet<const FunctionDecl *, 4> Seen;
};

void emitDirectCalleePrototypes(const FunctionDecl *FD,
                                AutoDiffEmitter::Mode Mode,
                                const StringSet<> &Existing,
                                const PrintingPolicy &Policy, raw_ostream &OS) {
  DirectAutoDiffCalleeCollector Collector;
  Collector.TraverseStmt(const_cast<Stmt *>(FD->getBody()));
  for (const FunctionDecl *Callee : Collector.Callees) {
    if (isa<CXXMethodDecl>(Callee))
      continue;
    const auto *Attr = getAutoDiffAttr(Callee);
    bool WantsMode =
        Mode == AutoDiffEmitter::Fwd ? Attr->hasForward() : Attr->hasBackward();
    if (!WantsMode || Existing.count(Callee->getName()))
      continue;
    SmallVector<ActiveContextInfo, 4> Contexts = buildActiveContexts(Callee);
    emitAutoDiffSignature(Callee, Mode,
                          printType(Callee->getReturnType(), Policy), Contexts,
                          Policy, OS);
    OS << ";\n";
  }
}

SmallVector<StringRef, 4> getSourceNamespacePath(const FunctionDecl *FD) {
  SmallVector<StringRef, 4> Reversed;
  for (const DeclContext *DC = FD->getDeclContext(); DC; DC = DC->getParent()) {
    const auto *NS = dyn_cast<NamespaceDecl>(DC);
    if (NS && NS->getIdentifier())
      Reversed.push_back(NS->getName());
  }
  SmallVector<StringRef, 4> Path;
  for (auto It = Reversed.rbegin(); It != Reversed.rend(); ++It)
    Path.push_back(*It);
  return Path;
}

void openGeneratedNamespaces(ArrayRef<StringRef> SourcePath,
                             AutoDiffEmitter::Mode Mode, raw_ostream &OS) {
  OS << "\nnamespace user { namespace ad { namespace "
     << (Mode == AutoDiffEmitter::Fwd ? "fwd" : "bwd") << " { ";
  for (StringRef Name : SourcePath)
    OS << "namespace " << Name << " { ";
  OS << "\n";
}

void closeGeneratedNamespaces(ArrayRef<StringRef> SourcePath,
                              AutoDiffEmitter::Mode Mode, raw_ostream &OS) {
  for (size_t I = 0; I < SourcePath.size(); ++I)
    OS << "} ";
  OS << "} } } // namespace user::ad::"
     << (Mode == AutoDiffEmitter::Fwd ? "fwd" : "bwd");
  for (StringRef Name : SourcePath)
    OS << "::" << Name;
  OS << "\n";
}

// Emit forward and/or backward generated functions for a single annotated
// function, wrapped in the user::ad::{fwd,bwd}:: namespaces.
//
// If a function with the same unqualified name already exists in the
// corresponding user::ad::fwd / user::ad::bwd namespace (see
// collectUserAdFunctionNames) the corresponding mode is skipped, leaving
// the user's existing implementation untouched.
void EmitAutoDiffForFunction(const FunctionDecl *FD,
                             const HLSLAutoDiffAttr *Attr,
                             const StringSet<> &ExistingFwd,
                             const StringSet<> &ExistingBwd,
                             const PrintingPolicy &Policy, raw_ostream &OS) {
  StringRef Name = FD->getName();
  SmallVector<StringRef, 4> SourcePath = getSourceNamespacePath(FD);
  if (Attr->hasForward() && !ExistingFwd.count(Name)) {
    openGeneratedNamespaces(SourcePath, AutoDiffEmitter::Fwd, OS);
    OS << "using namespace ::ad::fwd;\n";
    emitDirectCalleePrototypes(FD, AutoDiffEmitter::Fwd, ExistingFwd, Policy,
                               OS);
    emitAutoDiffFunction(FD, AutoDiffEmitter::Fwd, Policy, OS);
    closeGeneratedNamespaces(SourcePath, AutoDiffEmitter::Fwd, OS);
  }
  if (Attr->hasBackward() && !ExistingBwd.count(Name)) {
    openGeneratedNamespaces(SourcePath, AutoDiffEmitter::Bwd, OS);
    OS << "using namespace ::ad::bwd;\n";
    emitDirectCalleePrototypes(FD, AutoDiffEmitter::Bwd, ExistingBwd, Policy,
                               OS);
    emitAutoDiffFunction(FD, AutoDiffEmitter::Bwd, Policy, OS);
    closeGeneratedNamespaces(SourcePath, AutoDiffEmitter::Bwd, OS);
  }
}

// Return true if any method of \p RD carries HLSLAutoDiffAttr (any mode).
bool recordHasAutoDiffMember(const CXXRecordDecl *RD) {
  for (const Decl *D : RD->decls()) {
    if (const auto *MD = dyn_cast<CXXMethodDecl>(D))
      if (MD->hasAttr<HLSLAutoDiffAttr>())
        return true;
  }
  return false;
}

// Collect the names of every method already declared inside the user's
// `user::ad::{fwd,bwd}::ClassName` record (if any). Mirrors the per-namespace
// skip used for free functions, scoped to a specific wrapper class.
void collectUserAdMethodNames(const CXXRecordDecl *UserAdRD,
                              StringSet<> &MethodNames) {
  if (!UserAdRD)
    return;
  for (const Decl *D : UserAdRD->decls()) {
    if (const auto *MD = dyn_cast<CXXMethodDecl>(D)) {
      if (MD->getIdentifier())
        MethodNames.insert(MD->getName());
    }
  }
}

// Emit the auto-diff wrapper class for \p RD, containing a generated method
// per HLSLAutoDiffAttr-bearing member function. The wrapper is named the same
// as the source class and inherits from it, so existing member data and
// non-differentiable methods remain accessible.
//
// Forward and backward modes are emitted in separate
// `namespace user { ad { fwd|bwd } }` blocks, each containing one
// `struct ClassName : ::ClassName { ... };` definition. A mode is skipped
// entirely if no method of \p RD requests that mode, or if the user has
// already supplied a wrapper class for the corresponding mode. (Partial /
// per-method merging with a user-supplied wrapper class is a follow-up.)
void EmitAutoDiffForRecord(const CXXRecordDecl *RD,
                           const CXXRecordDecl *UserFwdWrapper,
                           const CXXRecordDecl *UserBwdWrapper,
                           const PrintingPolicy &Policy, raw_ostream &OS) {
  StringRef ClassName = RD->getName();

  // Determine which modes are actually requested by any method.
  bool AnyFwd = false;
  bool AnyBwd = false;
  for (const Decl *D : RD->decls()) {
    const auto *MD = dyn_cast<CXXMethodDecl>(D);
    if (!MD)
      continue;
    if (const auto *AD = MD->getAttr<HLSLAutoDiffAttr>()) {
      AnyFwd |= AD->hasForward();
      AnyBwd |= AD->hasBackward();
    }
  }

  auto EmitMode = [&](AutoDiffEmitter::Mode Mode, const char *NS,
                      const CXXRecordDecl *UserWrapper) {
    OS << "\nnamespace user { namespace ad { namespace " << NS << " {\n";
    OS << "using namespace ::ad::" << NS << ";\n";
    OS << "struct " << ClassName << " : ::" << ClassName << " {\n";
    StringSet<> ExistingMethods;
    collectUserAdMethodNames(UserWrapper, ExistingMethods);
    for (const Decl *D : RD->decls()) {
      const auto *MD = dyn_cast<CXXMethodDecl>(D);
      if (!MD)
        continue;
      const auto *AD = MD->getAttr<HLSLAutoDiffAttr>();
      if (!AD) {
        continue;
      }
      bool WantsThisMode =
          (Mode == AutoDiffEmitter::Fwd) ? AD->hasForward() : AD->hasBackward();
      if (!WantsThisMode)
        continue;
      if (ExistingMethods.count(MD->getName()))
        continue;
      // Methods sit inside the wrapper class, so indent one level.
      OS << "  ";
      emitAutoDiffFunction(MD, Mode, Policy, OS);
    }
    OS << "};\n";
    OS << "} } } // namespace user::ad::" << NS << "\n";
  };

  if (AnyFwd && !UserFwdWrapper)
    EmitMode(AutoDiffEmitter::Fwd, "fwd", UserFwdWrapper);
  if (AnyBwd && !UserBwdWrapper)
    EmitMode(AutoDiffEmitter::Bwd, "bwd", UserBwdWrapper);
}

// Build the textual replacement for a user-supplied `user::ad::{fwd,bwd}::C`
// wrapper class, augmented with auto-generated methods for every annotated
// source-class method that the user has not implemented.
//
// The returned string contains a `using namespace ::ad::{fwd,bwd};` directive
// (so generated bodies do not need to fully qualify Value<T>/Variable<T>) and
// a single complete `struct C : <user bases> { ... };` definition. It is
// substituted in-place for the user's CXXRecordDecl during translation-unit
// printing.
std::string buildMergedWrapperClass(const CXXRecordDecl *UserRD,
                                    const CXXRecordDecl *SrcRD,
                                    AutoDiffEmitter::Mode Mode,
                                    const PrintingPolicy &Policy) {
  std::string Result;
  raw_string_ostream OS(Result);
  const char *NS = (Mode == AutoDiffEmitter::Fwd) ? "fwd" : "bwd";
  OS << "using namespace ::ad::" << NS << ";\n";
  OS << (UserRD->isStruct() ? "struct " : "class ") << UserRD->getName();
  // Preserve the user's base specifier list verbatim. Auto-diff convention
  // is `: ::C`; we do not invent or alter inheritance.
  bool First = true;
  for (const auto &B : UserRD->bases()) {
    OS << (First ? " : " : ", ");
    First = false;
    if (B.isVirtual())
      OS << "virtual ";
    switch (B.getAccessSpecifierAsWritten()) {
    case AS_public:
      OS << "public ";
      break;
    case AS_protected:
      OS << "protected ";
      break;
    case AS_private:
      OS << "private ";
      break;
    case AS_none:
      break;
    }
    B.getType().print(OS, Policy);
  }
  OS << " {\n";
  // Pass through every non-implicit user declaration inside the wrapper.
  // This preserves access specifiers, fields, typedefs, and the user's
  // hand-written methods exactly as they were typed.
  for (const Decl *D : UserRD->decls()) {
    if (D->isImplicit())
      continue;
    OS << "  ";
    D->print(OS, Policy);
    OS << "\n";
  }
  // Append auto-generated bodies for any annotated source method whose name
  // the user has not already declared inside the wrapper.
  StringSet<> Existing;
  collectUserAdMethodNames(UserRD, Existing);
  for (const Decl *D : SrcRD->decls()) {
    const auto *MD = dyn_cast<CXXMethodDecl>(D);
    if (!MD)
      continue;
    const auto *AD = MD->getAttr<HLSLAutoDiffAttr>();
    if (!AD)
      continue;
    bool Wants =
        (Mode == AutoDiffEmitter::Fwd) ? AD->hasForward() : AD->hasBackward();
    if (!Wants)
      continue;
    if (Existing.count(MD->getName()))
      continue;
    OS << "  ";
    emitAutoDiffFunction(MD, Mode, Policy, OS);
  }
  OS << "};\n";
  OS.flush();
  return Result;
}

// Recursively print \p D, substituting any CXXRecordDecl found in \p Subs
// with the precomputed merged class text. NamespaceDecls are descended into
// so that a record buried inside `user::ad::fwd` is reached; all other
// decl kinds are forwarded to clang's standard DeclPrinter.
void printDeclWithSubstitutions(
    const Decl *D, const DenseMap<const CXXRecordDecl *, std::string> &Subs,
    raw_ostream &OS, const PrintingPolicy &Policy) {
  if (D->isImplicit())
    return;
  if (const auto *NS = dyn_cast<NamespaceDecl>(D)) {
    OS << "namespace ";
    if (NS->getIdentifier())
      OS << NS->getName() << " ";
    OS << "{\n";
    for (const Decl *Child : NS->decls()) {
      printDeclWithSubstitutions(Child, Subs, OS, Policy);
    }
    OS << "}\n";
    return;
  }
  if (const auto *RD = dyn_cast<CXXRecordDecl>(D)) {
    auto It = Subs.find(RD);
    if (It != Subs.end()) {
      OS << It->second;
      return;
    }
  }
  D->print(OS, Policy);
  const auto *Function = dyn_cast<FunctionDecl>(D);
  if (isa<TagDecl>(D) || isa<VarDecl>(D) ||
      (Function && !Function->doesThisDeclarationHaveABody()))
    OS << ";";
  OS << "\n";
}

void collectAnnotatedFreeFunctions(
    const DeclContext *DC, SmallVectorImpl<const FunctionDecl *> &Functions,
    SmallPtrSetImpl<const FunctionDecl *> &Seen) {
  for (const Decl *D : DC->decls()) {
    if (const auto *NS = dyn_cast<NamespaceDecl>(D)) {
      collectAnnotatedFreeFunctions(NS, Functions, Seen);
      continue;
    }
    const auto *FD = dyn_cast<FunctionDecl>(D);
    if (!FD || isa<CXXMethodDecl>(FD) || !FD->doesThisDeclarationHaveABody() ||
        !FD->hasAttr<HLSLAutoDiffAttr>())
      continue;
    const FunctionDecl *Canonical = FD->getCanonicalDecl();
    if (Seen.insert(Canonical).second)
      Functions.push_back(FD);
  }
}

void orderAnnotatedFunctionDependencies(
    const FunctionDecl *FD, SmallVectorImpl<const FunctionDecl *> &Ordered,
    SmallPtrSetImpl<const FunctionDecl *> &Visiting,
    SmallPtrSetImpl<const FunctionDecl *> &Emitted) {
  const FunctionDecl *Canonical = FD->getCanonicalDecl();
  if (Emitted.count(Canonical) || Visiting.count(Canonical))
    return;
  Visiting.insert(Canonical);

  DirectAutoDiffCalleeCollector Collector;
  Collector.TraverseStmt(const_cast<Stmt *>(FD->getBody()));
  for (const FunctionDecl *Callee : Collector.Callees) {
    if (isa<CXXMethodDecl>(Callee))
      continue;
    const FunctionDecl *Definition = nullptr;
    if (Callee->hasBody(Definition))
      orderAnnotatedFunctionDependencies(Definition, Ordered, Visiting,
                                         Emitted);
  }

  Visiting.erase(Canonical);
  if (Emitted.insert(Canonical).second)
    Ordered.push_back(FD);
}

} // anonymous namespace

namespace hlsl {

bool PrintTranslationUnitWithDifferentials(TranslationUnitDecl *tu,
                                           raw_ostream &OS,
                                           PrintingPolicy &Policy,
                                           bool GeneratedOnly,
                                           raw_ostream &Errors) {
  // Collect the names of any user::ad::fwd / user::ad::bwd functions the
  // input translation unit has already declared. We use these sets to skip
  // regenerating differentials a user has either hand-written or previously
  // checked in.
  StringSet<> ExistingFwd;
  StringSet<> ExistingBwd;
  collectUserAdFunctionNames(tu, ExistingFwd, ExistingBwd);

  // Determine which auto-diff library headers the generated output needs
  // and emit the corresponding #include directives at the very top of the
  // file. The wrapped namespace blocks pull names in with a
  // `using namespace ::ad::{fwd,bwd};` directive each, so the rewritten
  // bodies can stay free of fully-qualified names. We only include a
  // header if we are actually about to emit at least one function for
  // that mode; functions whose user-provided differentials are already
  // present do not require us to drag the library in.
  // Build the substitution map for user-supplied wrapper classes. A
  // user::ad::fwd::C / user::ad::bwd::C record found in the input is
  // suppressed from the normal print path and replaced with a merged class
  // definition that retains the user's hand-written methods and appends
  // auto-generated bodies for any annotated source method the user did not
  // provide. Records that are not in this map are printed as-is by
  // printDeclWithSubstitutions.
  DenseMap<const CXXRecordDecl *, std::string> Subs;
  static const StringRef FwdPath[] = {"user", "ad", "fwd"};
  static const StringRef BwdPath[] = {"user", "ad", "bwd"};
  SmallVector<const FunctionDecl *, 16> AnnotatedFunctions;
  SmallPtrSet<const FunctionDecl *, 16> SeenFunctions;
  collectAnnotatedFreeFunctions(tu, AnnotatedFunctions, SeenFunctions);

  bool NeedFwd = false;
  bool NeedBwd = false;
  for (const FunctionDecl *FD : AnnotatedFunctions) {
    const auto *AD = FD->getAttr<HLSLAutoDiffAttr>();
    StringRef Name = FD->getName();
    NeedFwd |= AD->hasForward() && !ExistingFwd.count(Name);
    NeedBwd |= AD->hasBackward() && !ExistingBwd.count(Name);
  }
  for (Decl *D : tu->decls()) {
    if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
      continue;
    }
    // Classes / structs whose methods carry HLSLAutoDiffAttr also trigger
    // generation of a wrapper class, which references the AD library types.
    if (const auto *RD = dyn_cast<CXXRecordDecl>(D)) {
      if (!RD->isCompleteDefinition() || !RD->getIdentifier())
        continue;
      if (!recordHasAutoDiffMember(RD))
        continue;
      const CXXRecordDecl *UserFwd =
          findUserAdRecord(tu, FwdPath, RD->getName());
      const CXXRecordDecl *UserBwd =
          findUserAdRecord(tu, BwdPath, RD->getName());
      StringSet<> FwdMethods, BwdMethods;
      collectUserAdMethodNames(UserFwd, FwdMethods);
      collectUserAdMethodNames(UserBwd, BwdMethods);
      bool NeedFwdMerge = false;
      bool NeedBwdMerge = false;
      for (const Decl *Inner : RD->decls()) {
        const auto *MD = dyn_cast<CXXMethodDecl>(Inner);
        if (!MD)
          continue;
        const auto *AD = MD->getAttr<HLSLAutoDiffAttr>();
        if (!AD)
          continue;
        bool MissingFwd = AD->hasForward() && !FwdMethods.count(MD->getName());
        bool MissingBwd = AD->hasBackward() && !BwdMethods.count(MD->getName());
        NeedFwd |= MissingFwd;
        NeedBwd |= MissingBwd;
        NeedFwdMerge |= MissingFwd && UserFwd;
        NeedBwdMerge |= MissingBwd && UserBwd;
      }
      if (GeneratedOnly && (NeedFwdMerge || NeedBwdMerge)) {
        Errors << "-generated-only cannot merge generated methods into "
                  "authored differential wrapper '"
               << RD->getName() << "'; use -generate-differentials without "
                                   "-generated-only to merge the wrapper\n";
        return false;
      }
      if (UserFwd && NeedFwdMerge)
        Subs[UserFwd] =
            buildMergedWrapperClass(UserFwd, RD, AutoDiffEmitter::Fwd, Policy);
      if (UserBwd && NeedBwdMerge)
        Subs[UserBwd] =
            buildMergedWrapperClass(UserBwd, RD, AutoDiffEmitter::Bwd, Policy);
    }
  }
  if (NeedFwd)
    OS << "#include <ad/fwd>\n";
  if (NeedBwd)
    OS << "#include <ad/bwd>\n";
  if (NeedFwd || NeedBwd)
    OS << "\n";

  for (Decl *D : tu->decls()) {
    if (D->isImplicit())
      continue;
    if (!GeneratedOnly)
      printDeclWithSubstitutions(D, Subs, OS, Policy);
    if (isa<FunctionDecl>(D)) {
      continue;
    }
    if (auto *RD = dyn_cast<CXXRecordDecl>(D)) {
      if (!RD->isCompleteDefinition() || !RD->getIdentifier())
        continue;
      if (!recordHasAutoDiffMember(RD))
        continue;
      const CXXRecordDecl *UserFwd =
          findUserAdRecord(tu, FwdPath, RD->getName());
      const CXXRecordDecl *UserBwd =
          findUserAdRecord(tu, BwdPath, RD->getName());
      // EmitAutoDiffForRecord skips a mode whose user wrapper is present;
      // partial-merge cases are handled by the substitution above, so the
      // post-record emit only runs for "no user wrapper at all" modes.
      EmitAutoDiffForRecord(RD, UserFwd, UserBwd, Policy, OS);
    }
  }
  SmallVector<const FunctionDecl *, 16> OrderedFunctions;
  SmallPtrSet<const FunctionDecl *, 16> VisitingFunctions;
  SmallPtrSet<const FunctionDecl *, 16> EmittedFunctions;
  for (const FunctionDecl *FD : AnnotatedFunctions)
    orderAnnotatedFunctionDependencies(FD, OrderedFunctions, VisitingFunctions,
                                       EmittedFunctions);
  for (const FunctionDecl *FD : OrderedFunctions)
    EmitAutoDiffForFunction(FD, FD->getAttr<HLSLAutoDiffAttr>(), ExistingFwd,
                            ExistingBwd, Policy, OS);
  return true;
}

} // namespace hlsl
