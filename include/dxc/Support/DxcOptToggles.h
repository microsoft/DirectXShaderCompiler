///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcOptToggles.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Helper code for representing -opt-disable, -opt-enable, -opt-select       //
// options                                                                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef LLVM_HLSL_DXC_OPT_TOGGLES_H
#define LLVM_HLSL_DXC_OPT_TOGGLES_H

#include "llvm/ADT/StringRef.h"
#include <map>
#include <set>
#include <string>

namespace hlsl {

namespace options {

static const llvm::StringRef TOGGLE_GVN  = "gvn";
static const llvm::StringRef TOGGLE_LICM = "licm";
static const llvm::StringRef TOGGLE_SINK  = "sink";
static const llvm::StringRef TOGGLE_LIFETIME_MARKERS = "lifetime-markers";
static const llvm::StringRef TOGGLE_PARTIAL_LIFETIME_MARKERS = "partial-lifetime-markers";
static const llvm::StringRef TOGGLE_STRUCTURIZE_LOOP_EXITS_FOR_UNROLL = "structurize-loop-exits-for-unroll";
static const llvm::StringRef TOGGLE_DEBUG_NOPS = "debug-nops";
static const llvm::StringRef TOGGLE_STRUCTURIZE_RETURNS = "structurize-returns";

struct OptimizationToggles {
  // Optimization pass enables, disables and selects
  std::map<std::string, bool>        Toggles; // OPT_opt_enable & OPT_opt_disable
  std::map<std::string, std::string> Selects; // OPT_opt_select

  inline void Set(llvm::StringRef Opt, bool Value) {
    Toggles[Opt] = Value;
  }
  inline bool SetAndTrue(llvm::StringRef Opt) const {
    auto It = Toggles.find(Opt);
    return It != Toggles.end() && It->second;
  }
  inline bool SetAndFalse(llvm::StringRef Opt) const {
    auto It = Toggles.find(Opt);
    return It != Toggles.end() && !It->second;
  }
  inline bool Get(llvm::StringRef Opt, bool DefaultOn) const {
    auto It = Toggles.find(Opt);
    const bool Found = It != Toggles.end();
    if (DefaultOn) {
      return !Found || It->second;
    }
    else {
      return Found && It->second;
    }
  }
  inline bool GetDefaultOn(llvm::StringRef Opt) const {
    return Get(Opt, /*DefaultOn*/true);
  }
  inline bool GetDefaultOff(llvm::StringRef Opt) const {
    return Get(Opt, /*DefaultOn*/false);
  }
};

} // namespace options
} // namespace hlsl

#endif
