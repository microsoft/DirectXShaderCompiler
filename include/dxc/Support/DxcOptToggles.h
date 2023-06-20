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

struct Toggle {
  llvm::StringRef Name;
  bool Default = false;
  Toggle(llvm::StringRef Name, bool Default) : Name(Name), Default(Default) {}
};

static const Toggle TOGGLE_GVN = {"gvn", true};
static const Toggle TOGGLE_LICM = {"licm", true};
static const Toggle TOGGLE_SINK = {"sink", true};
static const Toggle TOGGLE_LIFETIME_MARKERS = {"lifetime-markers", false};
static const Toggle TOGGLE_PARTIAL_LIFETIME_MARKERS = {
    "partial-lifetime-markers", false};
static const Toggle TOGGLE_STRUCTURIZE_LOOP_EXITS_FOR_UNROLL = {
    "structurize-loop-exits-for-unroll", true};
static const Toggle TOGGLE_DEBUG_NOPS = {"debug-nops", true};
static const Toggle TOGGLE_STRUCTURIZE_RETURNS = {"structurize-returns", false};

struct OptimizationToggles {
  // Optimization pass enables, disables and selects
  std::map<std::string, bool>        Toggles; // OPT_opt_enable & OPT_opt_disable
  std::map<std::string, std::string> Selects; // OPT_opt_select

  void Set(Toggle Opt, bool Value) {
    Toggles[Opt.Name] = Value;
  }
  bool Has(Toggle Opt) const {
    return Toggles.find(Opt.Name) != Toggles.end();
  }
  bool Get(Toggle Opt) const {
    auto It = Toggles.find(Opt.Name);
    const bool Found = It != Toggles.end();
    if (Found)
      return It->second;
    return Opt.Default;
  }
};

} // namespace options
} // namespace hlsl

#endif
