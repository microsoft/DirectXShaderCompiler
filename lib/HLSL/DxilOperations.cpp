///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilOperations.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implementation of DXIL operation tables.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilOperations.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/HLModule.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;
using std::vector;
using std::string;


namespace hlsl {

using OC = OP::OpCode;
using OCC = OP::OpCodeClass;

//------------------------------------------------------------------------------
//
//  OP class const-static data and related static methods.
//
/* <py>
import hctdb_instrhelp
</py> */
/* <py::lines('OPCODE-OLOADS')>hctdb_instrhelp.get_oloads_props()</py>*/
// OPCODE-OLOADS:BEGIN
const OP::OpCodeProperty OP::m_OpCodeProps[(unsigned)OP::OpCode::NumOpCodes] = {
//   OpCode                       OpCode name,                OpCodeClass                    OpCodeClass name,              void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  // Temporary, indexable, input, output registers                                                                          void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::TempRegLoad,             "TempRegLoad",              OCC::TempRegLoad,              "tempRegLoad",               {false,  true,  true, false, false, false,  true,  true, false}, Attribute::ReadOnly, },
  {  OC::TempRegStore,            "TempRegStore",             OCC::TempRegStore,             "tempRegStore",              {false,  true,  true, false, false, false,  true,  true, false}, Attribute::None,     },
  {  OC::MinPrecXRegLoad,         "MinPrecXRegLoad",          OCC::MinPrecXRegLoad,          "minPrecXRegLoad",           {false,  true, false, false, false, false,  true, false, false}, Attribute::ReadOnly, },
  {  OC::MinPrecXRegStore,        "MinPrecXRegStore",         OCC::MinPrecXRegStore,         "minPrecXRegStore",          {false,  true, false, false, false, false,  true, false, false}, Attribute::None,     },
  {  OC::LoadInput,               "LoadInput",                OCC::LoadInput,                "loadInput",                 {false,  true,  true, false, false, false,  true,  true, false}, Attribute::ReadNone, },
  {  OC::StoreOutput,             "StoreOutput",              OCC::StoreOutput,              "storeOutput",               {false,  true,  true, false, false, false,  true,  true, false}, Attribute::None,     },

  // Unary float                                                                                                            void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::FAbs,                    "FAbs",                     OCC::Unary,                    "unary",                     {false,  true,  true,  true, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Saturate,                "Saturate",                 OCC::Unary,                    "unary",                     {false,  true,  true,  true, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::IsNaN,                   "IsNaN",                    OCC::IsSpecialFloat,           "isSpecialFloat",            {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::IsInf,                   "IsInf",                    OCC::IsSpecialFloat,           "isSpecialFloat",            {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::IsFinite,                "IsFinite",                 OCC::IsSpecialFloat,           "isSpecialFloat",            {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::IsNormal,                "IsNormal",                 OCC::IsSpecialFloat,           "isSpecialFloat",            {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Cos,                     "Cos",                      OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Sin,                     "Sin",                      OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Tan,                     "Tan",                      OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Acos,                    "Acos",                     OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Asin,                    "Asin",                     OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Atan,                    "Atan",                     OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Hcos,                    "Hcos",                     OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Hsin,                    "Hsin",                     OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Htan,                    "Htan",                     OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Exp,                     "Exp",                      OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Frc,                     "Frc",                      OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Log,                     "Log",                      OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Sqrt,                    "Sqrt",                     OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Rsqrt,                   "Rsqrt",                    OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },

  // Unary float - rounding                                                                                                 void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::Round_ne,                "Round_ne",                 OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Round_ni,                "Round_ni",                 OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Round_pi,                "Round_pi",                 OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Round_z,                 "Round_z",                  OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },

  // Unary int                                                                                                              void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::Bfrev,                   "Bfrev",                    OCC::Unary,                    "unary",                     {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },
  {  OC::Countbits,               "Countbits",                OCC::UnaryBits,                "unaryBits",                 {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },
  {  OC::FirstbitLo,              "FirstbitLo",               OCC::UnaryBits,                "unaryBits",                 {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },

  // Unary uint                                                                                                             void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::FirstbitHi,              "FirstbitHi",               OCC::UnaryBits,                "unaryBits",                 {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },

  // Unary int                                                                                                              void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::FirstbitSHi,             "FirstbitSHi",              OCC::UnaryBits,                "unaryBits",                 {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },

  // Binary float                                                                                                           void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::FMax,                    "FMax",                     OCC::Binary,                   "binary",                    {false,  true,  true,  true, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::FMin,                    "FMin",                     OCC::Binary,                   "binary",                    {false,  true,  true,  true, false, false, false, false, false}, Attribute::ReadNone, },

  // Binary int                                                                                                             void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::IMax,                    "IMax",                     OCC::Binary,                   "binary",                    {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },
  {  OC::IMin,                    "IMin",                     OCC::Binary,                   "binary",                    {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },

  // Binary uint                                                                                                            void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::UMax,                    "UMax",                     OCC::Binary,                   "binary",                    {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },
  {  OC::UMin,                    "UMin",                     OCC::Binary,                   "binary",                    {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },

  // Binary int with two outputs                                                                                            void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::IMul,                    "IMul",                     OCC::BinaryWithTwoOuts,        "binaryWithTwoOuts",         {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },

  // Binary uint with two outputs                                                                                           void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::UMul,                    "UMul",                     OCC::BinaryWithTwoOuts,        "binaryWithTwoOuts",         {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },
  {  OC::UDiv,                    "UDiv",                     OCC::BinaryWithTwoOuts,        "binaryWithTwoOuts",         {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },

  // Binary uint with carry or borrow                                                                                       void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::UAddc,                   "UAddc",                    OCC::BinaryWithCarryOrBorrow,  "binaryWithCarryOrBorrow",   {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },
  {  OC::USubb,                   "USubb",                    OCC::BinaryWithCarryOrBorrow,  "binaryWithCarryOrBorrow",   {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },

  // Tertiary float                                                                                                         void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::FMad,                    "FMad",                     OCC::Tertiary,                 "tertiary",                  {false,  true,  true,  true, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Fma,                     "Fma",                      OCC::Tertiary,                 "tertiary",                  {false, false, false,  true, false, false, false, false, false}, Attribute::ReadNone, },

  // Tertiary int                                                                                                           void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::IMad,                    "IMad",                     OCC::Tertiary,                 "tertiary",                  {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },

  // Tertiary uint                                                                                                          void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::UMad,                    "UMad",                     OCC::Tertiary,                 "tertiary",                  {false, false, false, false, false, false,  true,  true,  true}, Attribute::ReadNone, },

  // Tertiary int                                                                                                           void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::Msad,                    "Msad",                     OCC::Tertiary,                 "tertiary",                  {false, false, false, false, false, false, false,  true,  true}, Attribute::ReadNone, },
  {  OC::Ibfe,                    "Ibfe",                     OCC::Tertiary,                 "tertiary",                  {false, false, false, false, false, false, false,  true,  true}, Attribute::ReadNone, },

  // Tertiary uint                                                                                                          void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::Ubfe,                    "Ubfe",                     OCC::Tertiary,                 "tertiary",                  {false, false, false, false, false, false, false,  true,  true}, Attribute::ReadNone, },

  // Quaternary                                                                                                             void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::Bfi,                     "Bfi",                      OCC::Quaternary,               "quaternary",                {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },

  // Dot                                                                                                                    void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::Dot2,                    "Dot2",                     OCC::Dot2,                     "dot2",                      {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Dot3,                    "Dot3",                     OCC::Dot3,                     "dot3",                      {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::Dot4,                    "Dot4",                     OCC::Dot4,                     "dot4",                      {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },

  // Resources                                                                                                              void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::CreateHandle,            "CreateHandle",             OCC::CreateHandle,             "createHandle",              { true, false, false, false, false, false, false, false, false}, Attribute::ReadOnly, },
  {  OC::CBufferLoad,             "CBufferLoad",              OCC::CBufferLoad,              "cbufferLoad",               {false,  true,  true,  true, false,  true,  true,  true,  true}, Attribute::ReadOnly, },
  {  OC::CBufferLoadLegacy,       "CBufferLoadLegacy",        OCC::CBufferLoadLegacy,        "cbufferLoadLegacy",         {false,  true,  true,  true, false, false,  true,  true,  true}, Attribute::ReadOnly, },

  // Resources - sample                                                                                                     void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::Sample,                  "Sample",                   OCC::Sample,                   "sample",                    {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadOnly, },
  {  OC::SampleBias,              "SampleBias",               OCC::SampleBias,               "sampleBias",                {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadOnly, },
  {  OC::SampleLevel,             "SampleLevel",              OCC::SampleLevel,              "sampleLevel",               {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadOnly, },
  {  OC::SampleGrad,              "SampleGrad",               OCC::SampleGrad,               "sampleGrad",                {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadOnly, },
  {  OC::SampleCmp,               "SampleCmp",                OCC::SampleCmp,                "sampleCmp",                 {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadOnly, },
  {  OC::SampleCmpLevelZero,      "SampleCmpLevelZero",       OCC::SampleCmpLevelZero,       "sampleCmpLevelZero",        {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadOnly, },

  // Resources                                                                                                              void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::TextureLoad,             "TextureLoad",              OCC::TextureLoad,              "textureLoad",               {false,  true,  true, false, false, false,  true,  true, false}, Attribute::ReadOnly, },
  {  OC::TextureStore,            "TextureStore",             OCC::TextureStore,             "textureStore",              {false,  true,  true, false, false, false,  true,  true, false}, Attribute::None,     },
  {  OC::BufferLoad,              "BufferLoad",               OCC::BufferLoad,               "bufferLoad",                {false,  true,  true, false, false, false,  true,  true, false}, Attribute::ReadOnly, },
  {  OC::BufferStore,             "BufferStore",              OCC::BufferStore,              "bufferStore",               {false,  true,  true, false, false, false,  true,  true, false}, Attribute::None,     },
  {  OC::BufferUpdateCounter,     "BufferUpdateCounter",      OCC::BufferUpdateCounter,      "bufferUpdateCounter",       { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::CheckAccessFullyMapped,  "CheckAccessFullyMapped",   OCC::CheckAccessFullyMapped,   "checkAccessFullyMapped",    {false, false, false, false, false, false, false,  true, false}, Attribute::ReadOnly, },
  {  OC::GetDimensions,           "GetDimensions",            OCC::GetDimensions,            "getDimensions",             { true, false, false, false, false, false, false, false, false}, Attribute::ReadOnly, },

  // Resources - gather                                                                                                     void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::TextureGather,           "TextureGather",            OCC::TextureGather,            "textureGather",             {false,  true,  true, false, false, false,  true,  true, false}, Attribute::ReadOnly, },
  {  OC::TextureGatherCmp,        "TextureGatherCmp",         OCC::TextureGatherCmp,         "textureGatherCmp",          {false,  true,  true, false, false, false,  true,  true, false}, Attribute::ReadOnly, },

  // Resources - sample                                                                                                     void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::Texture2DMSGetSamplePosition, "Texture2DMSGetSamplePosition", OCC::Texture2DMSGetSamplePosition, "texture2DMSGetSamplePosition",   {true, false, false, false, false, false, false, false, false}, Attribute::ReadOnly, },
  {  OC::RenderTargetGetSamplePosition, "RenderTargetGetSamplePosition", OCC::RenderTargetGetSamplePosition, "renderTargetGetSamplePosition",   {true, false, false, false, false, false, false, false, false}, Attribute::ReadOnly, },
  {  OC::RenderTargetGetSampleCount, "RenderTargetGetSampleCount", OCC::RenderTargetGetSampleCount, "renderTargetGetSampleCount",   {true, false, false, false, false, false, false, false, false}, Attribute::ReadOnly, },

  // Synchronization                                                                                                        void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::AtomicBinOp,             "AtomicBinOp",              OCC::AtomicBinOp,              "atomicBinOp",               {false, false, false, false, false, false, false,  true, false}, Attribute::None,     },
  {  OC::AtomicCompareExchange,   "AtomicCompareExchange",    OCC::AtomicCompareExchange,    "atomicCompareExchange",     {false, false, false, false, false, false, false,  true, false}, Attribute::None,     },
  {  OC::Barrier,                 "Barrier",                  OCC::Barrier,                  "barrier",                   { true, false, false, false, false, false, false, false, false}, Attribute::NoDuplicate, },

  // Pixel shader                                                                                                           void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::CalculateLOD,            "CalculateLOD",             OCC::CalculateLOD,             "calculateLOD",              {false, false,  true, false, false, false, false, false, false}, Attribute::ReadOnly, },
  {  OC::Discard,                 "Discard",                  OCC::Discard,                  "discard",                   { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::DerivCoarseX,            "DerivCoarseX",             OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::DerivCoarseY,            "DerivCoarseY",             OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::DerivFineX,              "DerivFineX",               OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::DerivFineY,              "DerivFineY",               OCC::Unary,                    "unary",                     {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::EvalSnapped,             "EvalSnapped",              OCC::EvalSnapped,              "evalSnapped",               {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::EvalSampleIndex,         "EvalSampleIndex",          OCC::EvalSampleIndex,          "evalSampleIndex",           {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::EvalCentroid,            "EvalCentroid",             OCC::EvalCentroid,             "evalCentroid",              {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::SampleIndex,             "SampleIndex",              OCC::SampleIndex,              "sampleIndex",               {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },
  {  OC::Coverage,                "Coverage",                 OCC::Coverage,                 "coverage",                  {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },
  {  OC::InnerCoverage,           "InnerCoverage",            OCC::InnerCoverage,            "innerCoverage",             {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },

  // Compute shader                                                                                                         void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::ThreadId,                "ThreadId",                 OCC::ThreadId,                 "threadId",                  {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },
  {  OC::GroupId,                 "GroupId",                  OCC::GroupId,                  "groupId",                   {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },
  {  OC::ThreadIdInGroup,         "ThreadIdInGroup",          OCC::ThreadIdInGroup,          "threadIdInGroup",           {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },
  {  OC::FlattenedThreadIdInGroup, "FlattenedThreadIdInGroup", OCC::FlattenedThreadIdInGroup, "flattenedThreadIdInGroup", {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },

  // Geometry shader                                                                                                        void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::EmitStream,              "EmitStream",               OCC::EmitStream,               "emitStream",                { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::CutStream,               "CutStream",                OCC::CutStream,                "cutStream",                 { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::EmitThenCutStream,       "EmitThenCutStream",        OCC::EmitThenCutStream,        "emitThenCutStream",         { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::GSInstanceID,            "GSInstanceID",             OCC::GSInstanceID,             "gsInstanceID",              {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },

  // Double precision                                                                                                       void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::MakeDouble,              "MakeDouble",               OCC::MakeDouble,               "makeDouble",                {false, false, false,  true, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::SplitDouble,             "SplitDouble",              OCC::SplitDouble,              "splitDouble",               {false, false, false,  true, false, false, false, false, false}, Attribute::ReadNone, },

  // Domain and hull shader                                                                                                 void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::LoadOutputControlPoint,  "LoadOutputControlPoint",   OCC::LoadOutputControlPoint,   "loadOutputControlPoint",    {false,  true,  true, false, false, false,  true,  true, false}, Attribute::ReadNone, },
  {  OC::LoadPatchConstant,       "LoadPatchConstant",        OCC::LoadPatchConstant,        "loadPatchConstant",         {false,  true,  true, false, false, false,  true,  true, false}, Attribute::ReadNone, },

  // Domain shader                                                                                                          void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::DomainLocation,          "DomainLocation",           OCC::DomainLocation,           "domainLocation",            {false, false,  true, false, false, false, false, false, false}, Attribute::ReadNone, },

  // Hull shader                                                                                                            void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::StorePatchConstant,      "StorePatchConstant",       OCC::StorePatchConstant,       "storePatchConstant",        {false,  true,  true, false, false, false,  true,  true, false}, Attribute::None,     },
  {  OC::OutputControlPointID,    "OutputControlPointID",     OCC::OutputControlPointID,     "outputControlPointID",      {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },
  {  OC::PrimitiveID,             "PrimitiveID",              OCC::PrimitiveID,              "primitiveID",               {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },

  // Other                                                                                                                  void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::CycleCounterLegacy,      "CycleCounterLegacy",       OCC::CycleCounterLegacy,       "cycleCounterLegacy",        { true, false, false, false, false, false, false, false, false}, Attribute::None,     },

  // Wave                                                                                                                   void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::WaveIsFirstLane,         "WaveIsFirstLane",          OCC::WaveIsFirstLane,          "waveIsFirstLane",           { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::WaveGetLaneIndex,        "WaveGetLaneIndex",         OCC::WaveGetLaneIndex,         "waveGetLaneIndex",          { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::WaveGetLaneCount,        "WaveGetLaneCount",         OCC::WaveGetLaneCount,         "waveGetLaneCount",          { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::WaveAnyTrue,             "WaveAnyTrue",              OCC::WaveAnyTrue,              "waveAnyTrue",               { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::WaveAllTrue,             "WaveAllTrue",              OCC::WaveAllTrue,              "waveAllTrue",               { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::WaveActiveAllEqual,      "WaveActiveAllEqual",       OCC::WaveActiveAllEqual,       "waveActiveAllEqual",        {false,  true,  true,  true,  true,  true,  true,  true,  true}, Attribute::None,     },
  {  OC::WaveActiveBallot,        "WaveActiveBallot",         OCC::WaveActiveBallot,         "waveActiveBallot",          { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::WaveReadLaneAt,          "WaveReadLaneAt",           OCC::WaveReadLaneAt,           "waveReadLaneAt",            {false,  true,  true,  true,  true,  true,  true,  true,  true}, Attribute::None,     },
  {  OC::WaveReadLaneFirst,       "WaveReadLaneFirst",        OCC::WaveReadLaneFirst,        "waveReadLaneFirst",         {false,  true,  true, false,  true,  true,  true,  true,  true}, Attribute::None,     },
  {  OC::WaveActiveOp,            "WaveActiveOp",             OCC::WaveActiveOp,             "waveActiveOp",              {false,  true,  true,  true,  true,  true,  true,  true,  true}, Attribute::None,     },
  {  OC::WaveActiveBit,           "WaveActiveBit",            OCC::WaveActiveBit,            "waveActiveBit",             {false, false, false, false, false,  true,  true,  true,  true}, Attribute::None,     },
  {  OC::WavePrefixOp,            "WavePrefixOp",             OCC::WavePrefixOp,             "wavePrefixOp",              {false,  true,  true,  true, false,  true,  true,  true,  true}, Attribute::None,     },
  {  OC::QuadReadLaneAt,          "QuadReadLaneAt",           OCC::QuadReadLaneAt,           "quadReadLaneAt",            {false,  true,  true,  true,  true,  true,  true,  true,  true}, Attribute::None,     },
  {  OC::QuadOp,                  "QuadOp",                   OCC::QuadOp,                   "quadOp",                    {false,  true,  true,  true, false,  true,  true,  true,  true}, Attribute::None,     },

  // Bitcasts with different sizes                                                                                          void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::BitcastI16toF16,         "BitcastI16toF16",          OCC::BitcastI16toF16,          "bitcastI16toF16",           { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::BitcastF16toI16,         "BitcastF16toI16",          OCC::BitcastF16toI16,          "bitcastF16toI16",           { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::BitcastI32toF32,         "BitcastI32toF32",          OCC::BitcastI32toF32,          "bitcastI32toF32",           { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::BitcastF32toI32,         "BitcastF32toI32",          OCC::BitcastF32toI32,          "bitcastF32toI32",           { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::BitcastI64toF64,         "BitcastI64toF64",          OCC::BitcastI64toF64,          "bitcastI64toF64",           { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::BitcastF64toI64,         "BitcastF64toI64",          OCC::BitcastF64toI64,          "bitcastF64toI64",           { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },

  // Legacy floating-point                                                                                                  void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::LegacyF32ToF16,          "LegacyF32ToF16",           OCC::LegacyF32ToF16,           "legacyF32ToF16",            { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::LegacyF16ToF32,          "LegacyF16ToF32",           OCC::LegacyF16ToF32,           "legacyF16ToF32",            { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },

  // Double precision                                                                                                       void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::LegacyDoubleToFloat,     "LegacyDoubleToFloat",      OCC::LegacyDoubleToFloat,      "legacyDoubleToFloat",       { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::LegacyDoubleToSInt32,    "LegacyDoubleToSInt32",     OCC::LegacyDoubleToSInt32,     "legacyDoubleToSInt32",      { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },
  {  OC::LegacyDoubleToUInt32,    "LegacyDoubleToUInt32",     OCC::LegacyDoubleToUInt32,     "legacyDoubleToUInt32",      { true, false, false, false, false, false, false, false, false}, Attribute::ReadNone, },

  // Wave                                                                                                                   void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::WaveAllBitCount,         "WaveAllBitCount",          OCC::WaveAllOp,                "waveAllOp",                 { true, false, false, false, false, false, false, false, false}, Attribute::None,     },
  {  OC::WavePrefixBitCount,      "WavePrefixBitCount",       OCC::WavePrefixOp,             "wavePrefixOp",              { true, false, false, false, false, false, false, false, false}, Attribute::None,     },

  // Pixel shader                                                                                                           void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::AttributeAtVertex,       "AttributeAtVertex",        OCC::AttributeAtVertex,        "attributeAtVertex",         {false,  true,  true, false, false, false, false, false, false}, Attribute::ReadNone, },

  // Graphics shader                                                                                                        void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::ViewID,                  "ViewID",                   OCC::ViewID,                   "viewID",                    {false, false, false, false, false, false, false,  true, false}, Attribute::ReadNone, },

  // Resources                                                                                                              void,     h,     f,     d,    i1,    i8,   i16,   i32,   i64  function attribute
  {  OC::RawBufferLoad,           "RawBufferLoad",            OCC::RawBufferLoad,            "rawBufferLoad",             {false,  true,  true, false, false, false,  true,  true, false}, Attribute::ReadOnly, },
  {  OC::RawBufferStore,          "RawBufferStore",           OCC::RawBufferStore,           "rawBufferStore",            {false,  true,  true, false, false, false,  true,  true, false}, Attribute::None,     },
};
// OPCODE-OLOADS:END

const char *OP::m_OverloadTypeName[kNumTypeOverloads] = {
  "void", "f16", "f32", "f64", "i1", "i8", "i16", "i32", "i64"
};

const char *OP::m_NamePrefix = "dx.op.";
const char *OP::m_TypePrefix = "dx.types.";
const char *OP::m_MatrixTypePrefix = "class.matrix."; // Allowed in library

// Keep sync with DXIL::AtomicBinOpCode
static const char *AtomicBinOpCodeName[] = {
    "AtomicAdd",
    "AtomicAnd",
    "AtomicOr",
    "AtomicXor",
    "AtomicIMin",
    "AtomicIMax",
    "AtomicUMin",
    "AtomicUMax",
    "AtomicExchange",
    "AtomicInvalid"           // Must be last.
};

unsigned OP::GetTypeSlot(Type *pType) {
  Type::TypeID T = pType->getTypeID();
  switch (T) {
  case Type::VoidTyID:    return 0;
  case Type::HalfTyID:    return 1;
  case Type::FloatTyID:   return 2;
  case Type::DoubleTyID:  return 3;
  case Type::IntegerTyID: {
    IntegerType *pIT = dyn_cast<IntegerType>(pType);
    unsigned Bits = pIT->getBitWidth();
    switch (Bits) {
    case 1:               return 4;
    case 8:               return 5;
    case 16:              return 6;
    case 32:              return 7;
    case 64:              return 8;
    }
  }
  default:
    break;
  }
  return UINT_MAX;
}

const char *OP::GetOverloadTypeName(unsigned TypeSlot) {
  DXASSERT(TypeSlot < kNumTypeOverloads, "otherwise caller passed OOB index");
  return m_OverloadTypeName[TypeSlot];
}

const char *OP::GetOpCodeName(OpCode opCode) {
  DXASSERT(0 <= (unsigned)opCode && opCode < OpCode::NumOpCodes, "otherwise caller passed OOB index");
  return m_OpCodeProps[(unsigned)opCode].pOpCodeName;
}

const char *OP::GetAtomicOpName(DXIL::AtomicBinOpCode OpCode) {
  unsigned opcode = static_cast<unsigned>(OpCode);
  DXASSERT_LOCALVAR(opcode, opcode < static_cast<unsigned>(DXIL::AtomicBinOpCode::Invalid), "otherwise caller passed OOB index");
  return AtomicBinOpCodeName[static_cast<unsigned>(OpCode)];
}

OP::OpCodeClass OP::GetOpCodeClass(OpCode opCode) {
  DXASSERT(0 <= (unsigned)opCode && opCode < OpCode::NumOpCodes, "otherwise caller passed OOB index");
  return m_OpCodeProps[(unsigned)opCode].opCodeClass;
}

const char *OP::GetOpCodeClassName(OpCode opCode) {
  DXASSERT(0 <= (unsigned)opCode && opCode < OpCode::NumOpCodes, "otherwise caller passed OOB index");
  return m_OpCodeProps[(unsigned)opCode].pOpCodeClassName;
}

bool OP::IsOverloadLegal(OpCode opCode, Type *pType) {
  DXASSERT(0 <= (unsigned)opCode && opCode < OpCode::NumOpCodes, "otherwise caller passed OOB index");
  unsigned TypeSlot = GetTypeSlot(pType);
  return TypeSlot != UINT_MAX && m_OpCodeProps[(unsigned)opCode].bAllowOverload[TypeSlot];
}

bool OP::CheckOpCodeTable() {
  for (unsigned i = 0; i < (unsigned)OpCode::NumOpCodes; i++) {
    if ((unsigned)m_OpCodeProps[i].opCode != i)
      return false;
  }

  return true;
}

bool OP::IsDxilOpFuncName(StringRef name) {
  return name.startswith(OP::m_NamePrefix);
}

bool OP::IsDxilOpFunc(const llvm::Function *F) {
  if (!F->hasName())
    return false;
  return IsDxilOpFuncName(F->getName());
}

bool OP::IsDxilOpTypeName(StringRef name) {
  return name.startswith(m_TypePrefix) || name.startswith(m_MatrixTypePrefix);
}

bool OP::IsDxilOpType(llvm::StructType *ST) {
  if (!ST->hasName())
    return false;
  StringRef Name = ST->getName();
  return IsDxilOpTypeName(Name);
}

bool OP::IsDupDxilOpType(llvm::StructType *ST) {
  if (!ST->hasName())
    return false;
  StringRef Name = ST->getName();
  if (!IsDxilOpTypeName(Name))
    return false;
  size_t DotPos = Name.rfind('.');
  if (DotPos == 0 || DotPos == StringRef::npos || Name.back() == '.' ||
      !isdigit(static_cast<unsigned char>(Name[DotPos + 1])))
    return false;
  return true;
}

StructType *OP::GetOriginalDxilOpType(llvm::StructType *ST, llvm::Module &M) {
  DXASSERT(IsDupDxilOpType(ST), "else should not call GetOriginalDxilOpType");
  StringRef Name = ST->getName();
  size_t DotPos = Name.rfind('.');
  StructType *OriginalST = M.getTypeByName(Name.substr(0, DotPos));
  DXASSERT(OriginalST, "else name collison without original type");
  DXASSERT(ST->isLayoutIdentical(OriginalST),
           "else invalid layout for dxil types");
  return OriginalST;
}

bool OP::IsDxilOpFuncCallInst(const llvm::Instruction *I) {
  const CallInst *CI = dyn_cast<CallInst>(I);
  if (CI == nullptr) return false;
  return IsDxilOpFunc(CI->getCalledFunction());
}

bool OP::IsDxilOpFuncCallInst(const llvm::Instruction *I, OpCode opcode) {
  if (!IsDxilOpFuncCallInst(I)) return false;
  return llvm::cast<llvm::ConstantInt>(I->getOperand(0))->getZExtValue() == (unsigned)opcode;
}

OP::OpCode OP::GetDxilOpFuncCallInst(const llvm::Instruction *I) {
  DXASSERT(IsDxilOpFuncCallInst(I), "else caller didn't call IsDxilOpFuncCallInst to check");
  return (OP::OpCode)llvm::cast<llvm::ConstantInt>(I->getOperand(0))->getZExtValue();
}

bool OP::IsDxilOpWave(OpCode C) {
  unsigned op = (unsigned)C;
  /* <py::lines('OPCODE-WAVE')>hctdb_instrhelp.get_instrs_pred("op", "is_wave")</py>*/
  // OPCODE-WAVE:BEGIN
  // Instructions: WaveIsFirstLane=110, WaveGetLaneIndex=111,
  // WaveGetLaneCount=112, WaveAnyTrue=113, WaveAllTrue=114,
  // WaveActiveAllEqual=115, WaveActiveBallot=116, WaveReadLaneAt=117,
  // WaveReadLaneFirst=118, WaveActiveOp=119, WaveActiveBit=120,
  // WavePrefixOp=121, QuadReadLaneAt=122, QuadOp=123, WaveAllBitCount=135,
  // WavePrefixBitCount=136
  return (110 <= op && op <= 123) || (135 <= op && op <= 136);
  // OPCODE-WAVE:END
}

bool OP::IsDxilOpGradient(OpCode C) {
  unsigned op = (unsigned)C;
  /* <py::lines('OPCODE-GRADIENT')>hctdb_instrhelp.get_instrs_pred("op", "is_gradient")</py>*/
  // OPCODE-GRADIENT:BEGIN
  // Instructions: Sample=60, SampleBias=61, SampleCmp=64, TextureGather=73,
  // TextureGatherCmp=74, CalculateLOD=81, DerivCoarseX=83, DerivCoarseY=84,
  // DerivFineX=85, DerivFineY=86
  return (60 <= op && op <= 61) || op == 64 || (73 <= op && op <= 74) || op == 81 || (83 <= op && op <= 86);
  // OPCODE-GRADIENT:END
}

static Type *GetOrCreateStructType(LLVMContext &Ctx, ArrayRef<Type*> types, StringRef Name, Module *pModule) {
  if (StructType *ST = pModule->getTypeByName(Name)) {
    // TODO: validate the exist type match types if needed.
    return ST;
  }
  else
    return StructType::create(Ctx, types, Name);
}

//------------------------------------------------------------------------------
//
//  OP methods.
//
OP::OP(LLVMContext &Ctx, Module *pModule)
: m_Ctx(Ctx)
, m_pModule(pModule)
, m_LowPrecisionMode(DXIL::LowPrecisionMode::Undefined) {
  memset(m_pResRetType, 0, sizeof(m_pResRetType));
  memset(m_pCBufferRetType, 0, sizeof(m_pCBufferRetType));
  memset(m_OpCodeClassCache, 0, sizeof(m_OpCodeClassCache));
  static_assert(_countof(OP::m_OpCodeProps) == (size_t)OP::OpCode::NumOpCodes, "forgot to update OP::m_OpCodeProps");

  m_pHandleType = GetOrCreateStructType(m_Ctx, Type::getInt8PtrTy(m_Ctx), "dx.types.Handle", pModule);

  Type *DimsType[4] = { Type::getInt32Ty(m_Ctx), Type::getInt32Ty(m_Ctx), Type::getInt32Ty(m_Ctx), Type::getInt32Ty(m_Ctx) };
  m_pDimensionsType = GetOrCreateStructType(m_Ctx, DimsType, "dx.types.Dimensions", pModule);

  Type *SamplePosType[2] = { Type::getFloatTy(m_Ctx), Type::getFloatTy(m_Ctx) };
  m_pSamplePosType = GetOrCreateStructType(m_Ctx, SamplePosType, "dx.types.SamplePos", pModule);

  Type *I32cTypes[2] = { Type::getInt32Ty(m_Ctx), Type::getInt1Ty(m_Ctx) };
  m_pBinaryWithCarryType = GetOrCreateStructType(m_Ctx, I32cTypes, "dx.types.i32c", pModule);

  Type *TwoI32Types[2] = { Type::getInt32Ty(m_Ctx), Type::getInt32Ty(m_Ctx) };
  m_pBinaryWithTwoOutputsType = GetOrCreateStructType(m_Ctx, TwoI32Types, "dx.types.twoi32", pModule);

  Type *SplitDoubleTypes[2] = { Type::getInt32Ty(m_Ctx), Type::getInt32Ty(m_Ctx) }; // Lo, Hi.
  m_pSplitDoubleType = GetOrCreateStructType(m_Ctx, SplitDoubleTypes, "dx.types.splitdouble", pModule);

  Type *Int4Types[4] = { Type::getInt32Ty(m_Ctx), Type::getInt32Ty(m_Ctx), Type::getInt32Ty(m_Ctx), Type::getInt32Ty(m_Ctx) }; // HiHi, HiLo, LoHi, LoLo
  m_pInt4Type = GetOrCreateStructType(m_Ctx, Int4Types, "dx.types.fouri32", pModule);
  // Try to find existing intrinsic function.
  RefreshCache();
}

void OP::RefreshCache() {
  for (Function &F : m_pModule->functions()) {
    if (OP::IsDxilOpFunc(&F) && !F.user_empty()) {
      CallInst *CI = cast<CallInst>(*F.user_begin());
      OpCode OpCode = OP::GetDxilOpFuncCallInst(CI);
      Type *pOverloadType = OP::GetOverloadType(OpCode, &F);
      Function *OpFunc = GetOpFunc(OpCode, pOverloadType);
      (void)(OpFunc);
      DXASSERT_NOMSG(OpFunc == &F);
    }
  }
}

void OP::UpdateCache(OpCodeClass opClass, unsigned typeSlot, llvm::Function *F) {
  m_OpCodeClassCache[(unsigned)opClass].pOverloads[typeSlot] = F;
  m_FunctionToOpClass[F] = opClass;
}

Function *OP::GetOpFunc(OpCode opCode, Type *pOverloadType) {
  DXASSERT(0 <= (unsigned)opCode && opCode < OpCode::NumOpCodes, "otherwise caller passed OOB OpCode");
  _Analysis_assume_(0 <= (unsigned)opCode && opCode < OpCode::NumOpCodes);
  DXASSERT(IsOverloadLegal(opCode, pOverloadType), "otherwise the caller requested illegal operation overload (eg HLSL function with unsupported types for mapped intrinsic function)");
  unsigned TypeSlot = GetTypeSlot(pOverloadType);
  OpCodeClass opClass = m_OpCodeProps[(unsigned)opCode].opCodeClass;
  Function *&F = m_OpCodeClassCache[(unsigned)opClass].pOverloads[TypeSlot];
  if (F != nullptr) {
    UpdateCache(opClass, TypeSlot, F);
    return F;
  }

  vector<Type*> ArgTypes;      // RetType is ArgTypes[0]
  Type *pETy = pOverloadType;
  Type *pRes = GetHandleType();
  Type *pDim = GetDimensionsType();
  Type *pPos = GetSamplePosType();
  Type *pV = Type::getVoidTy(m_Ctx);
  Type *pI1 = Type::getInt1Ty(m_Ctx);
  Type *pI8 = Type::getInt8Ty(m_Ctx);
  Type *pI16 = Type::getInt16Ty(m_Ctx);
  Type *pI32 = Type::getInt32Ty(m_Ctx);
  Type *pPI32 = Type::getInt32PtrTy(m_Ctx); (void)(pPI32); // Currently unused.
  Type *pI64 = Type::getInt64Ty(m_Ctx); (void)(pI64); // Currently unused.
  Type *pF16 = Type::getHalfTy(m_Ctx);
  Type *pF32 = Type::getFloatTy(m_Ctx);
  Type *pPF32 = Type::getFloatPtrTy(m_Ctx);
  Type *pI32C = GetBinaryWithCarryType();
  Type *p2I32 = GetBinaryWithTwoOutputsType();
  Type *pF64 = Type::getDoubleTy(m_Ctx);
  Type *pSDT = GetSplitDoubleType();  // Split double type.
  Type *pI4S = GetInt4Type(); // 4 i32s in a struct.

  std::string funcName = (Twine(OP::m_NamePrefix) + Twine(GetOpCodeClassName(opCode))).str();
  // Add ret type to the name.
  if (pOverloadType != pV) {
    funcName = Twine(funcName).concat(".").concat(GetOverloadTypeName(TypeSlot)).str();
  } 
  // Try to find exist function with the same name in the module.
  if (Function *existF = m_pModule->getFunction(funcName)) {
    F = existF;
    UpdateCache(opClass, TypeSlot, F);
    return F;
  }

#define A(_x) ArgTypes.emplace_back(_x)
#define RRT(_y) A(GetResRetType(_y))
#define CBRT(_y) A(GetCBufferRetType(_y))

/* <py::lines('OPCODE-OLOAD-FUNCS')>hctdb_instrhelp.get_oloads_funcs()</py>*/
  switch (opCode) {            // return     opCode
// OPCODE-OLOAD-FUNCS:BEGIN
    // Temporary, indexable, input, output registers
  case OpCode::TempRegLoad:            A(pETy);     A(pI32); A(pI32); break;
  case OpCode::TempRegStore:           A(pV);       A(pI32); A(pI32); A(pETy); break;
  case OpCode::MinPrecXRegLoad:        A(pETy);     A(pI32); A(pPF32);A(pI32); A(pI8);  break;
  case OpCode::MinPrecXRegStore:       A(pV);       A(pI32); A(pPF32);A(pI32); A(pI8);  A(pETy); break;
  case OpCode::LoadInput:              A(pETy);     A(pI32); A(pI32); A(pI32); A(pI8);  A(pI32); break;
  case OpCode::StoreOutput:            A(pV);       A(pI32); A(pI32); A(pI32); A(pI8);  A(pETy); break;

    // Unary float
  case OpCode::FAbs:                   A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Saturate:               A(pETy);     A(pI32); A(pETy); break;
  case OpCode::IsNaN:                  A(pI1);      A(pI32); A(pETy); break;
  case OpCode::IsInf:                  A(pI1);      A(pI32); A(pETy); break;
  case OpCode::IsFinite:               A(pI1);      A(pI32); A(pETy); break;
  case OpCode::IsNormal:               A(pI1);      A(pI32); A(pETy); break;
  case OpCode::Cos:                    A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Sin:                    A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Tan:                    A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Acos:                   A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Asin:                   A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Atan:                   A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Hcos:                   A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Hsin:                   A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Htan:                   A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Exp:                    A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Frc:                    A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Log:                    A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Sqrt:                   A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Rsqrt:                  A(pETy);     A(pI32); A(pETy); break;

    // Unary float - rounding
  case OpCode::Round_ne:               A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Round_ni:               A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Round_pi:               A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Round_z:                A(pETy);     A(pI32); A(pETy); break;

    // Unary int
  case OpCode::Bfrev:                  A(pETy);     A(pI32); A(pETy); break;
  case OpCode::Countbits:              A(pI32);     A(pI32); A(pETy); break;
  case OpCode::FirstbitLo:             A(pI32);     A(pI32); A(pETy); break;

    // Unary uint
  case OpCode::FirstbitHi:             A(pI32);     A(pI32); A(pETy); break;

    // Unary int
  case OpCode::FirstbitSHi:            A(pI32);     A(pI32); A(pETy); break;

    // Binary float
  case OpCode::FMax:                   A(pETy);     A(pI32); A(pETy); A(pETy); break;
  case OpCode::FMin:                   A(pETy);     A(pI32); A(pETy); A(pETy); break;

    // Binary int
  case OpCode::IMax:                   A(pETy);     A(pI32); A(pETy); A(pETy); break;
  case OpCode::IMin:                   A(pETy);     A(pI32); A(pETy); A(pETy); break;

    // Binary uint
  case OpCode::UMax:                   A(pETy);     A(pI32); A(pETy); A(pETy); break;
  case OpCode::UMin:                   A(pETy);     A(pI32); A(pETy); A(pETy); break;

    // Binary int with two outputs
  case OpCode::IMul:                   A(p2I32);    A(pI32); A(pETy); A(pETy); break;

    // Binary uint with two outputs
  case OpCode::UMul:                   A(p2I32);    A(pI32); A(pETy); A(pETy); break;
  case OpCode::UDiv:                   A(p2I32);    A(pI32); A(pETy); A(pETy); break;

    // Binary uint with carry or borrow
  case OpCode::UAddc:                  A(pI32C);    A(pI32); A(pETy); A(pETy); break;
  case OpCode::USubb:                  A(pI32C);    A(pI32); A(pETy); A(pETy); break;

    // Tertiary float
  case OpCode::FMad:                   A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); break;
  case OpCode::Fma:                    A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); break;

    // Tertiary int
  case OpCode::IMad:                   A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); break;

    // Tertiary uint
  case OpCode::UMad:                   A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); break;

    // Tertiary int
  case OpCode::Msad:                   A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); break;
  case OpCode::Ibfe:                   A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); break;

    // Tertiary uint
  case OpCode::Ubfe:                   A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); break;

    // Quaternary
  case OpCode::Bfi:                    A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); A(pETy); break;

    // Dot
  case OpCode::Dot2:                   A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); A(pETy); break;
  case OpCode::Dot3:                   A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); A(pETy); A(pETy); A(pETy); break;
  case OpCode::Dot4:                   A(pETy);     A(pI32); A(pETy); A(pETy); A(pETy); A(pETy); A(pETy); A(pETy); A(pETy); A(pETy); break;

    // Resources
  case OpCode::CreateHandle:           A(pRes);     A(pI32); A(pI8);  A(pI32); A(pI32); A(pI1);  break;
  case OpCode::CBufferLoad:            A(pETy);     A(pI32); A(pRes); A(pI32); A(pI32); break;
  case OpCode::CBufferLoadLegacy:      CBRT(pETy);  A(pI32); A(pRes); A(pI32); break;

    // Resources - sample
  case OpCode::Sample:                 RRT(pETy);   A(pI32); A(pRes); A(pRes); A(pF32); A(pF32); A(pF32); A(pF32); A(pI32); A(pI32); A(pI32); A(pF32); break;
  case OpCode::SampleBias:             RRT(pETy);   A(pI32); A(pRes); A(pRes); A(pF32); A(pF32); A(pF32); A(pF32); A(pI32); A(pI32); A(pI32); A(pF32); A(pF32); break;
  case OpCode::SampleLevel:            RRT(pETy);   A(pI32); A(pRes); A(pRes); A(pF32); A(pF32); A(pF32); A(pF32); A(pI32); A(pI32); A(pI32); A(pF32); break;
  case OpCode::SampleGrad:             RRT(pETy);   A(pI32); A(pRes); A(pRes); A(pF32); A(pF32); A(pF32); A(pF32); A(pI32); A(pI32); A(pI32); A(pF32); A(pF32); A(pF32); A(pF32); A(pF32); A(pF32); A(pF32); break;
  case OpCode::SampleCmp:              RRT(pETy);   A(pI32); A(pRes); A(pRes); A(pF32); A(pF32); A(pF32); A(pF32); A(pI32); A(pI32); A(pI32); A(pF32); A(pF32); break;
  case OpCode::SampleCmpLevelZero:     RRT(pETy);   A(pI32); A(pRes); A(pRes); A(pF32); A(pF32); A(pF32); A(pF32); A(pI32); A(pI32); A(pI32); A(pF32); break;

    // Resources
  case OpCode::TextureLoad:            RRT(pETy);   A(pI32); A(pRes); A(pI32); A(pI32); A(pI32); A(pI32); A(pI32); A(pI32); A(pI32); break;
  case OpCode::TextureStore:           A(pV);       A(pI32); A(pRes); A(pI32); A(pI32); A(pI32); A(pETy); A(pETy); A(pETy); A(pETy); A(pI8);  break;
  case OpCode::BufferLoad:             RRT(pETy);   A(pI32); A(pRes); A(pI32); A(pI32); break;
  case OpCode::BufferStore:            A(pV);       A(pI32); A(pRes); A(pI32); A(pI32); A(pETy); A(pETy); A(pETy); A(pETy); A(pI8);  break;
  case OpCode::BufferUpdateCounter:    A(pI32);     A(pI32); A(pRes); A(pI8);  break;
  case OpCode::CheckAccessFullyMapped: A(pI1);      A(pI32); A(pI32); break;
  case OpCode::GetDimensions:          A(pDim);     A(pI32); A(pRes); A(pI32); break;

    // Resources - gather
  case OpCode::TextureGather:          RRT(pETy);   A(pI32); A(pRes); A(pRes); A(pF32); A(pF32); A(pF32); A(pF32); A(pI32); A(pI32); A(pI32); break;
  case OpCode::TextureGatherCmp:       RRT(pETy);   A(pI32); A(pRes); A(pRes); A(pF32); A(pF32); A(pF32); A(pF32); A(pI32); A(pI32); A(pI32); A(pF32); break;

    // Resources - sample
  case OpCode::Texture2DMSGetSamplePosition:A(pPos);     A(pI32); A(pRes); A(pI32); break;
  case OpCode::RenderTargetGetSamplePosition:A(pPos);     A(pI32); A(pI32); break;
  case OpCode::RenderTargetGetSampleCount:A(pI32);     A(pI32); break;

    // Synchronization
  case OpCode::AtomicBinOp:            A(pI32);     A(pI32); A(pRes); A(pI32); A(pI32); A(pI32); A(pI32); A(pI32); break;
  case OpCode::AtomicCompareExchange:  A(pI32);     A(pI32); A(pRes); A(pI32); A(pI32); A(pI32); A(pI32); A(pI32); break;
  case OpCode::Barrier:                A(pV);       A(pI32); A(pI32); break;

    // Pixel shader
  case OpCode::CalculateLOD:           A(pF32);     A(pI32); A(pRes); A(pRes); A(pF32); A(pF32); A(pF32); A(pI1);  break;
  case OpCode::Discard:                A(pV);       A(pI32); A(pI1);  break;
  case OpCode::DerivCoarseX:           A(pETy);     A(pI32); A(pETy); break;
  case OpCode::DerivCoarseY:           A(pETy);     A(pI32); A(pETy); break;
  case OpCode::DerivFineX:             A(pETy);     A(pI32); A(pETy); break;
  case OpCode::DerivFineY:             A(pETy);     A(pI32); A(pETy); break;
  case OpCode::EvalSnapped:            A(pETy);     A(pI32); A(pI32); A(pI32); A(pI8);  A(pI32); A(pI32); break;
  case OpCode::EvalSampleIndex:        A(pETy);     A(pI32); A(pI32); A(pI32); A(pI8);  A(pI32); break;
  case OpCode::EvalCentroid:           A(pETy);     A(pI32); A(pI32); A(pI32); A(pI8);  break;
  case OpCode::SampleIndex:            A(pI32);     A(pI32); break;
  case OpCode::Coverage:               A(pI32);     A(pI32); break;
  case OpCode::InnerCoverage:          A(pI32);     A(pI32); break;

    // Compute shader
  case OpCode::ThreadId:               A(pI32);     A(pI32); A(pI32); break;
  case OpCode::GroupId:                A(pI32);     A(pI32); A(pI32); break;
  case OpCode::ThreadIdInGroup:        A(pI32);     A(pI32); A(pI32); break;
  case OpCode::FlattenedThreadIdInGroup:A(pI32);     A(pI32); break;

    // Geometry shader
  case OpCode::EmitStream:             A(pV);       A(pI32); A(pI8);  break;
  case OpCode::CutStream:              A(pV);       A(pI32); A(pI8);  break;
  case OpCode::EmitThenCutStream:      A(pV);       A(pI32); A(pI8);  break;
  case OpCode::GSInstanceID:           A(pI32);     A(pI32); break;

    // Double precision
  case OpCode::MakeDouble:             A(pF64);     A(pI32); A(pI32); A(pI32); break;
  case OpCode::SplitDouble:            A(pSDT);     A(pI32); A(pF64); break;

    // Domain and hull shader
  case OpCode::LoadOutputControlPoint: A(pETy);     A(pI32); A(pI32); A(pI32); A(pI8);  A(pI32); break;
  case OpCode::LoadPatchConstant:      A(pETy);     A(pI32); A(pI32); A(pI32); A(pI8);  break;

    // Domain shader
  case OpCode::DomainLocation:         A(pF32);     A(pI32); A(pI8);  break;

    // Hull shader
  case OpCode::StorePatchConstant:     A(pV);       A(pI32); A(pI32); A(pI32); A(pI8);  A(pETy); break;
  case OpCode::OutputControlPointID:   A(pI32);     A(pI32); break;
  case OpCode::PrimitiveID:            A(pI32);     A(pI32); break;

    // Other
  case OpCode::CycleCounterLegacy:     A(p2I32);    A(pI32); break;

    // Wave
  case OpCode::WaveIsFirstLane:        A(pI1);      A(pI32); break;
  case OpCode::WaveGetLaneIndex:       A(pI32);     A(pI32); break;
  case OpCode::WaveGetLaneCount:       A(pI32);     A(pI32); break;
  case OpCode::WaveAnyTrue:            A(pI1);      A(pI32); A(pI1);  break;
  case OpCode::WaveAllTrue:            A(pI1);      A(pI32); A(pI1);  break;
  case OpCode::WaveActiveAllEqual:     A(pI1);      A(pI32); A(pETy); break;
  case OpCode::WaveActiveBallot:       A(pI4S);     A(pI32); A(pI1);  break;
  case OpCode::WaveReadLaneAt:         A(pETy);     A(pI32); A(pETy); A(pI32); break;
  case OpCode::WaveReadLaneFirst:      A(pETy);     A(pI32); A(pETy); break;
  case OpCode::WaveActiveOp:           A(pETy);     A(pI32); A(pETy); A(pI8);  A(pI8);  break;
  case OpCode::WaveActiveBit:          A(pETy);     A(pI32); A(pETy); A(pI8);  break;
  case OpCode::WavePrefixOp:           A(pETy);     A(pI32); A(pETy); A(pI8);  A(pI8);  break;
  case OpCode::QuadReadLaneAt:         A(pETy);     A(pI32); A(pETy); A(pI32); break;
  case OpCode::QuadOp:                 A(pETy);     A(pI32); A(pETy); A(pI8);  break;

    // Bitcasts with different sizes
  case OpCode::BitcastI16toF16:        A(pF16);     A(pI32); A(pI16); break;
  case OpCode::BitcastF16toI16:        A(pI16);     A(pI32); A(pF16); break;
  case OpCode::BitcastI32toF32:        A(pF32);     A(pI32); A(pI32); break;
  case OpCode::BitcastF32toI32:        A(pI32);     A(pI32); A(pF32); break;
  case OpCode::BitcastI64toF64:        A(pF64);     A(pI32); A(pI64); break;
  case OpCode::BitcastF64toI64:        A(pI64);     A(pI32); A(pF64); break;

    // Legacy floating-point
  case OpCode::LegacyF32ToF16:         A(pI32);     A(pI32); A(pF32); break;
  case OpCode::LegacyF16ToF32:         A(pF32);     A(pI32); A(pI32); break;

    // Double precision
  case OpCode::LegacyDoubleToFloat:    A(pF32);     A(pI32); A(pF64); break;
  case OpCode::LegacyDoubleToSInt32:   A(pI32);     A(pI32); A(pF64); break;
  case OpCode::LegacyDoubleToUInt32:   A(pI32);     A(pI32); A(pF64); break;

    // Wave
  case OpCode::WaveAllBitCount:        A(pI32);     A(pI32); A(pI1);  break;
  case OpCode::WavePrefixBitCount:     A(pI32);     A(pI32); A(pI1);  break;

    // Pixel shader
  case OpCode::AttributeAtVertex:      A(pETy);     A(pI32); A(pI32); A(pI32); A(pI8);  A(pI8);  break;

    // Graphics shader
  case OpCode::ViewID:                 A(pI32);     A(pI32); break;

    // Resources
  case OpCode::RawBufferLoad:          RRT(pETy);   A(pI32); A(pRes); A(pI32); A(pI32); A(pI8);  A(pI32); break;
  case OpCode::RawBufferStore:         A(pV);       A(pI32); A(pRes); A(pI32); A(pI32); A(pETy); A(pETy); A(pETy); A(pETy); A(pI8);  A(pI32); break;
  // OPCODE-OLOAD-FUNCS:END
  default: DXASSERT(false, "otherwise unhandled case"); break;
  }
#undef RRT
#undef A

  FunctionType *pFT;
  DXASSERT(ArgTypes.size() > 1, "otherwise forgot to initialize arguments");
  pFT = FunctionType::get(ArgTypes[0], ArrayRef<Type*>(&ArgTypes[1], ArgTypes.size()-1), false);

  F = cast<Function>(m_pModule->getOrInsertFunction(funcName, pFT));

  UpdateCache(opClass, TypeSlot, F);
  F->setCallingConv(CallingConv::C);
  F->addFnAttr(Attribute::NoUnwind);
  if (m_OpCodeProps[(unsigned)opCode].FuncAttr != Attribute::None)
    F->addFnAttr(m_OpCodeProps[(unsigned)opCode].FuncAttr);

  return F;
}

llvm::ArrayRef<llvm::Function *> OP::GetOpFuncList(OpCode opCode) const {
  DXASSERT(0 <= (unsigned)opCode && opCode < OpCode::NumOpCodes, "otherwise caller passed OOB OpCode");
  _Analysis_assume_(0 <= (unsigned)opCode && opCode < OpCode::NumOpCodes);
  return llvm::ArrayRef<llvm::Function *>(m_OpCodeClassCache[(unsigned)m_OpCodeProps[(unsigned)opCode].opCodeClass].pOverloads);
}

void OP::RemoveFunction(Function *F) {
  if (OP::IsDxilOpFunc(F)) {
    OpCodeClass opClass = m_FunctionToOpClass[F];
    for (unsigned i=0;i<kNumTypeOverloads;i++) {
      if (F == m_OpCodeClassCache[(unsigned)opClass].pOverloads[i]) {
        m_OpCodeClassCache[(unsigned)opClass].pOverloads[i] = nullptr;
        m_FunctionToOpClass.erase(F);
        break;
      }
    }
  }
}

bool OP::GetOpCodeClass(const Function *F, OP::OpCodeClass &opClass) {
  auto iter = m_FunctionToOpClass.find(F);
  if (iter == m_FunctionToOpClass.end()) {
    DXASSERT(!IsDxilOpFunc(F), "dxil function without an opcode class mapping?");
    return false;
  }
  opClass = iter->second;
  return true;
}

bool OP::UseMinPrecision() {
  if (m_LowPrecisionMode == DXIL::LowPrecisionMode::Undefined) {
    if (m_pModule->HasDxilModule()) {
      m_LowPrecisionMode = m_pModule->GetDxilModule().m_ShaderFlags.GetUseNativeLowPrecision() ?
        DXIL::LowPrecisionMode::UseNativeLowPrecision : DXIL::LowPrecisionMode::UseMinPrecision;
    }
    else if (m_pModule->HasHLModule()) {
      m_LowPrecisionMode = m_pModule->GetHLModule().GetHLOptions().bUseMinPrecision ?
        DXIL::LowPrecisionMode::UseMinPrecision : DXIL::LowPrecisionMode::UseNativeLowPrecision;
    }
    else {
      DXASSERT(false, "otherwise module doesn't contain either HLModule or Dxil Module.");
    }
  }
  return m_LowPrecisionMode == DXIL::LowPrecisionMode::UseMinPrecision;
}

uint64_t OP::GetAllocSizeForType(llvm::Type *Ty) {
  return m_pModule->getDataLayout().getTypeAllocSize(Ty);
}

llvm::Type *OP::GetOverloadType(OpCode opCode, llvm::Function *F) {
  DXASSERT(F, "not work on nullptr");
  Type *Ty = F->getReturnType();
  FunctionType *FT = F->getFunctionType();
/* <py::lines('OPCODE-OLOAD-TYPES')>hctdb_instrhelp.get_funcs_oload_type()</py>*/
  switch (opCode) {            // return     OpCode
  // OPCODE-OLOAD-TYPES:BEGIN
  case OpCode::TempRegStore:
    DXASSERT_NOMSG(FT->getNumParams() > 2);
    return FT->getParamType(2);
  case OpCode::MinPrecXRegStore:
  case OpCode::StoreOutput:
  case OpCode::BufferStore:
  case OpCode::StorePatchConstant:
  case OpCode::RawBufferStore:
    DXASSERT_NOMSG(FT->getNumParams() > 4);
    return FT->getParamType(4);
  case OpCode::IsNaN:
  case OpCode::IsInf:
  case OpCode::IsFinite:
  case OpCode::IsNormal:
  case OpCode::Countbits:
  case OpCode::FirstbitLo:
  case OpCode::FirstbitHi:
  case OpCode::FirstbitSHi:
  case OpCode::IMul:
  case OpCode::UMul:
  case OpCode::UDiv:
  case OpCode::UAddc:
  case OpCode::USubb:
  case OpCode::WaveActiveAllEqual:
    DXASSERT_NOMSG(FT->getNumParams() > 1);
    return FT->getParamType(1);
  case OpCode::TextureStore:
    DXASSERT_NOMSG(FT->getNumParams() > 5);
    return FT->getParamType(5);
  case OpCode::CreateHandle:
  case OpCode::BufferUpdateCounter:
  case OpCode::GetDimensions:
  case OpCode::Texture2DMSGetSamplePosition:
  case OpCode::RenderTargetGetSamplePosition:
  case OpCode::RenderTargetGetSampleCount:
  case OpCode::Barrier:
  case OpCode::Discard:
  case OpCode::EmitStream:
  case OpCode::CutStream:
  case OpCode::EmitThenCutStream:
  case OpCode::CycleCounterLegacy:
  case OpCode::WaveIsFirstLane:
  case OpCode::WaveGetLaneIndex:
  case OpCode::WaveGetLaneCount:
  case OpCode::WaveAnyTrue:
  case OpCode::WaveAllTrue:
  case OpCode::WaveActiveBallot:
  case OpCode::BitcastI16toF16:
  case OpCode::BitcastF16toI16:
  case OpCode::BitcastI32toF32:
  case OpCode::BitcastF32toI32:
  case OpCode::BitcastI64toF64:
  case OpCode::BitcastF64toI64:
  case OpCode::LegacyF32ToF16:
  case OpCode::LegacyF16ToF32:
  case OpCode::LegacyDoubleToFloat:
  case OpCode::LegacyDoubleToSInt32:
  case OpCode::LegacyDoubleToUInt32:
  case OpCode::WaveAllBitCount:
  case OpCode::WavePrefixBitCount:
    return Type::getVoidTy(m_Ctx);
  case OpCode::CheckAccessFullyMapped:
  case OpCode::AtomicBinOp:
  case OpCode::AtomicCompareExchange:
  case OpCode::SampleIndex:
  case OpCode::Coverage:
  case OpCode::InnerCoverage:
  case OpCode::ThreadId:
  case OpCode::GroupId:
  case OpCode::ThreadIdInGroup:
  case OpCode::FlattenedThreadIdInGroup:
  case OpCode::GSInstanceID:
  case OpCode::OutputControlPointID:
  case OpCode::PrimitiveID:
  case OpCode::ViewID:
    return IntegerType::get(m_Ctx, 32);
  case OpCode::CalculateLOD:
  case OpCode::DomainLocation:
    return Type::getFloatTy(m_Ctx);
  case OpCode::MakeDouble:
  case OpCode::SplitDouble:
    return Type::getDoubleTy(m_Ctx);
  case OpCode::CBufferLoadLegacy:
  case OpCode::Sample:
  case OpCode::SampleBias:
  case OpCode::SampleLevel:
  case OpCode::SampleGrad:
  case OpCode::SampleCmp:
  case OpCode::SampleCmpLevelZero:
  case OpCode::TextureLoad:
  case OpCode::BufferLoad:
  case OpCode::TextureGather:
  case OpCode::TextureGatherCmp:
  case OpCode::RawBufferLoad:
  {
    StructType *ST = cast<StructType>(Ty);
    return ST->getElementType(0);
  }
  // OPCODE-OLOAD-TYPES:END
  default: return Ty;
  }
}

Type *OP::GetHandleType() const {
  return m_pHandleType;
}

Type *OP::GetDimensionsType() const
{
  return m_pDimensionsType;
}

Type *OP::GetSamplePosType() const
{
  return m_pSamplePosType;
}

Type *OP::GetBinaryWithCarryType() const {
  return m_pBinaryWithCarryType;
}

Type *OP::GetBinaryWithTwoOutputsType() const {
  return m_pBinaryWithTwoOutputsType;
}

Type *OP::GetSplitDoubleType() const {
  return m_pSplitDoubleType;
}

Type *OP::GetInt4Type() const {
  return m_pInt4Type;
}

bool OP::IsResRetType(llvm::Type *Ty) {
  for (Type *ResTy : m_pResRetType) {
    if (Ty == ResTy)
      return true;
  }
  return false;
}

Type *OP::GetResRetType(Type *pOverloadType) {
  unsigned TypeSlot = GetTypeSlot(pOverloadType);

  if (m_pResRetType[TypeSlot] == nullptr) {
    string TypeName("dx.types.ResRet.");
    TypeName += GetOverloadTypeName(TypeSlot);
    Type *FieldTypes[5] = { pOverloadType, pOverloadType, pOverloadType, pOverloadType, Type::getInt32Ty(m_Ctx) };
    m_pResRetType[TypeSlot] = GetOrCreateStructType(m_Ctx, FieldTypes, TypeName, m_pModule);
  }

  return m_pResRetType[TypeSlot];
}

Type *OP::GetCBufferRetType(Type *pOverloadType) {
  unsigned TypeSlot = GetTypeSlot(pOverloadType);

  if (m_pCBufferRetType[TypeSlot] == nullptr) {
    string TypeName("dx.types.CBufRet.");
    TypeName += GetOverloadTypeName(TypeSlot);
    Type *i64Ty = Type::getInt64Ty(pOverloadType->getContext());
    Type *i16Ty = Type::getInt16Ty(pOverloadType->getContext());
    if (pOverloadType->isDoubleTy() || pOverloadType == i64Ty) {
      Type *FieldTypes[2] = { pOverloadType, pOverloadType };
      m_pCBufferRetType[TypeSlot] = GetOrCreateStructType(m_Ctx, FieldTypes, TypeName, m_pModule);
    }
    else if (!UseMinPrecision() && (pOverloadType->isHalfTy() || pOverloadType == i16Ty)) {
      TypeName += ".8"; // dx.types.CBufRet.fp16.8 for buffer of 8 halves
      Type *FieldTypes[8] = {
          pOverloadType, pOverloadType, pOverloadType, pOverloadType,
          pOverloadType, pOverloadType, pOverloadType, pOverloadType,
      };
      m_pCBufferRetType[TypeSlot] = GetOrCreateStructType(m_Ctx, FieldTypes, TypeName, m_pModule);
    }
    else {
      Type *FieldTypes[4] = { pOverloadType, pOverloadType, pOverloadType, pOverloadType };
      m_pCBufferRetType[TypeSlot] = GetOrCreateStructType(m_Ctx, FieldTypes, TypeName, m_pModule);
    }
  }
  return m_pCBufferRetType[TypeSlot];
}


//------------------------------------------------------------------------------
//
//  LLVM utility methods.
//
Constant *OP::GetI1Const(bool v) {
  return Constant::getIntegerValue(IntegerType::get(m_Ctx, 1), APInt(1, v));
}

Constant *OP::GetI8Const(char v) {
  return Constant::getIntegerValue(IntegerType::get(m_Ctx, 8), APInt(8, v));
}

Constant *OP::GetU8Const(unsigned char v) {
  return GetI8Const((char)v);
}

Constant *OP::GetI16Const(int v) {
  return Constant::getIntegerValue(IntegerType::get(m_Ctx, 16), APInt(16, v));
}

Constant *OP::GetU16Const(unsigned v) {
  return GetI16Const((int)v);
}

Constant *OP::GetI32Const(int v) {
  return Constant::getIntegerValue(IntegerType::get(m_Ctx, 32), APInt(32, v));
}

Constant *OP::GetU32Const(unsigned v) {
  return GetI32Const((int)v);
}

Constant *OP::GetU64Const(unsigned long long v) {
 return Constant::getIntegerValue(IntegerType::get(m_Ctx, 64), APInt(64, v));
}

Constant *OP::GetFloatConst(float v) {
  return ConstantFP::get(m_Ctx, APFloat(v));
}

Constant *OP::GetDoubleConst(double v) {
  return ConstantFP::get(m_Ctx, APFloat(v));
}

} // namespace hlsl
