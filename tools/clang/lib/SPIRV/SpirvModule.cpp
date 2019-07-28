//===--- SpirvModule.cpp - SPIR-V Module Implementation ----------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvModule.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

SpirvModule::SpirvModule()
    : capabilities({}), extensions({}), extInstSets({}), memoryModel(nullptr),
      entryPoints({}), executionModes({}), moduleProcesses({}), decorations({}),
      constants({}), variables({}), functions({}) {}

bool SpirvModule::invokeVisitor(Visitor *visitor, bool reverseOrder) {
  // Note: It is debatable whether reverse order of visiting the module should
  // reverse everything in this method. For the time being, we just reverse the
  // order of the function visitors, and keeping everything else the same.
  // For example, it is not clear what the value would be of vising the last
  // function first. We can update this methodology if needed.

  if (!visitor->visit(this, Visitor::Phase::Init))
    return false;

  if (reverseOrder) {
    // Reverse order of a SPIR-V module.

    // Our transformations do not cross function bounaries, therefore the order
    // of visiting functions is not important.
    for (auto iter = functions.rbegin(); iter != functions.rend(); ++iter) {
      auto *fn = *iter;
      if (!fn->invokeVisitor(visitor, reverseOrder))
        return false;
    }

    for (auto iter = variables.rbegin(); iter != variables.rend(); ++iter) {
      auto *var = *iter;
      if (!var->invokeVisitor(visitor))
        return false;
    }

    for (auto iter = constants.rbegin(); iter != constants.rend(); ++iter) {
      auto *constant = *iter;
      if (!constant->invokeVisitor(visitor))
        return false;
    }

    // Since SetVector doesn't have 'rbegin()' and 'rend()' methods, we use
    // manual indexing.
    for (auto decorIndex = decorations.size(); decorIndex > 0; --decorIndex) {
      auto *decoration = decorations[decorIndex - 1];
      if (!decoration->invokeVisitor(visitor))
        return false;
    }

    for (auto iter = moduleProcesses.rbegin(); iter != moduleProcesses.rend();
         ++iter) {
      auto *moduleProcess = *iter;
      if (!moduleProcess->invokeVisitor(visitor))
        return false;
    }

    if (!debugSources.empty())
      for (auto iter = debugSources.rbegin(); iter != debugSources.rend();
           ++iter) {
        auto *source = *iter;
        if (!source->invokeVisitor(visitor))
          return false;
      }

    for (auto iter = executionModes.rbegin(); iter != executionModes.rend();
         ++iter) {
      auto *execMode = *iter;
      if (!execMode->invokeVisitor(visitor))
        return false;
    }

    for (auto iter = entryPoints.rbegin(); iter != entryPoints.rend(); ++iter) {
      auto *entryPoint = *iter;
      if (!entryPoint->invokeVisitor(visitor))
        return false;
    }

    if (!memoryModel->invokeVisitor(visitor))
      return false;

    for (auto iter = extInstSets.rbegin(); iter != extInstSets.rend(); ++iter) {
      auto *extInstSet = *iter;
      if (!extInstSet->invokeVisitor(visitor))
        return false;
    }

    // Since SetVector doesn't have 'rbegin()' and 'rend()' methods, we use
    // manual indexing.
    for (auto extIndex = extensions.size(); extIndex > 0; --extIndex) {
      auto *extension = extensions[extIndex - 1];
      if (!extension->invokeVisitor(visitor))
        return false;
    }

    // Since SetVector doesn't have 'rbegin()' and 'rend()' methods, we use
    // manual indexing.
    for (auto capIndex = capabilities.size(); capIndex > 0; --capIndex) {
      auto *capability = capabilities[capIndex - 1];
      if (!capability->invokeVisitor(visitor))
        return false;
    }
  }
  // Traverse the regular order of a SPIR-V module.
  else {
    for (auto *cap : capabilities)
      if (!cap->invokeVisitor(visitor))
        return false;

    for (auto ext : extensions)
      if (!ext->invokeVisitor(visitor))
        return false;

    for (auto extInstSet : extInstSets)
      if (!extInstSet->invokeVisitor(visitor))
        return false;

    if (!memoryModel->invokeVisitor(visitor))
      return false;

    for (auto entryPoint : entryPoints)
      if (!entryPoint->invokeVisitor(visitor))
        return false;

    for (auto execMode : executionModes)
      if (!execMode->invokeVisitor(visitor))
        return false;

    if (!debugSources.empty())
      for (auto *source : debugSources)
        if (!source->invokeVisitor(visitor))
          return false;

    for (auto moduleProcess : moduleProcesses)
      if (!moduleProcess->invokeVisitor(visitor))
        return false;

    for (auto decoration : decorations)
      if (!decoration->invokeVisitor(visitor))
        return false;

    for (auto constant : constants)
      if (!constant->invokeVisitor(visitor))
        return false;

    for (auto var : variables)
      if (!var->invokeVisitor(visitor))
        return false;

    for (auto fn : functions)
      if (!fn->invokeVisitor(visitor, reverseOrder))
        return false;
  }

  if (!visitor->visit(this, Visitor::Phase::Done))
    return false;

  return true;
}

void SpirvModule::addFunction(SpirvFunction *fn) {
  assert(fn && "cannot add null function to the module");
  functions.push_back(fn);
}

void SpirvModule::addCapability(SpirvCapability *cap) {
  assert(cap && "cannot add null capability to the module");
  capabilities.insert(cap);
}

void SpirvModule::setMemoryModel(SpirvMemoryModel *model) {
  assert(model && "cannot set a null memory model");
  memoryModel = model;
}

void SpirvModule::addEntryPoint(SpirvEntryPoint *ep) {
  assert(ep && "cannot add null as an entry point");
  entryPoints.push_back(ep);
}

void SpirvModule::addExecutionMode(SpirvExecutionMode *em) {
  assert(em && "cannot add null execution mode");
  executionModes.push_back(em);
}

void SpirvModule::addExtension(SpirvExtension *ext) {
  assert(ext && "cannot add null extension");
  extensions.insert(ext);
}

void SpirvModule::addExtInstSet(SpirvExtInstImport *set) {
  assert(set && "cannot add null extended instruction set");
  extInstSets.push_back(set);
}

SpirvExtInstImport *SpirvModule::getGLSLExtInstSet() {
  // We expect very few (usually 1) extended instruction sets to exist in the
  // module, so this is not expensive.
  auto found =
      std::find_if(extInstSets.begin(), extInstSets.end(),
                   [](const SpirvExtInstImport *set) {
                     return set->getExtendedInstSetName() == "GLSL.std.450";
                   });

  if (found != extInstSets.end())
    return *found;

  return nullptr;
}

void SpirvModule::addVariable(SpirvVariable *var) {
  assert(var && "cannot add null variable to the module");
  variables.push_back(var);
}

void SpirvModule::addDecoration(SpirvDecoration *decor) {
  assert(decor && "cannot add null decoration to the module");
  decorations.insert(decor);
}

void SpirvModule::addConstant(SpirvConstant *constant) {
  assert(constant);
  constants.push_back(constant);
}

void SpirvModule::addDebugSource(SpirvSource *src) {
  assert(src);
  debugSources.push_back(src);
}

void SpirvModule::addModuleProcessed(SpirvModuleProcessed *p) {
  assert(p);
  moduleProcesses.push_back(p);
}

} // end namespace spirv
} // end namespace clang
