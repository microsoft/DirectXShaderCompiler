//===- PassPrinters.h - Utilities to print analysis info for passes -------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PassPrinters.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///
/// \file                                                                    //
/// \brief Utilities to print analysis info for various kinds of passes.     //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#ifndef LLVM_TOOLS_OPT_PASSPRINTERS_H
#define LLVM_TOOLS_OPT_PASSPRINTERS_H

namespace llvm {

class BasicBlockPass;
class CallGraphSCCPass;
class FunctionPass;
class ModulePass;
class LoopPass;
class PassInfo;
class RegionPass;
class raw_ostream;

FunctionPass *createFunctionPassPrinter(const PassInfo *PI, raw_ostream &out,
                                        bool Quiet);

CallGraphSCCPass *createCallGraphPassPrinter(const PassInfo *PI,
                                             raw_ostream &out, bool Quiet);

ModulePass *createModulePassPrinter(const PassInfo *PI, raw_ostream &out,
                                    bool Quiet);

LoopPass *createLoopPassPrinter(const PassInfo *PI, raw_ostream &out,
                                bool Quiet);

RegionPass *createRegionPassPrinter(const PassInfo *PI, raw_ostream &out,
                                    bool Quiet);

BasicBlockPass *createBasicBlockPassPrinter(const PassInfo *PI,
                                            raw_ostream &out, bool Quiet);
}

#endif // LLVM_TOOLS_OPT_PASSPRINTERS_H
