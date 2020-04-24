

#ifndef LLVM_ANALYSIS_LIVEVALUEANALYSIS_H
#define LLVM_ANALYSIS_LIVEVALUEANALYSIS_H

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <map>
#include <sstream>
#include <iomanip>


namespace llvm {

  class Function;
  class ModulePass;

  class LiveValueAnalysis : public ModulePass {

  public:

  private:

    llvm::Module* m_module = nullptr;
    std::set<CallInst *> m_callSites;
    std::map<CallInst *, std::vector<Instruction *>> m_spillsPerTraceCall;
    std::string m_rootPath;
    std::string m_outputFile;

  public:
    LiveValueAnalysis(StringRef LiveValueAnalysisOutputFile = "");
    ~LiveValueAnalysis() override;
    static char ID;

    bool runOnModule(Module &) override;

    void analyzeCFG();
    bool canRemat( Instruction* inst );
    std::string formatSourceLocation( const std::string &RootPath, const std::string &Location, int lineNumber );
    void formatOutput( raw_string_ostream &PrettyStr, raw_string_ostream &VSStr );
    template<class T, class C> std::vector<T> sortInstructions( C &c );
    std::string translateValueToName( DbgValueInst *DVI, Instruction *I );
    bool usesUavResource( CallInst *call );
  
  };

} // End llvm namespace

#endif
