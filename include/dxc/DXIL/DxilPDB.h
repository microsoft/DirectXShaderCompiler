#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"

namespace llvm {
  class raw_ostream;
}

namespace hlsl {
namespace pdb {

  void WriteDxilPDB(llvm::ArrayRef<char> Data, llvm::SmallString<32> Hash, llvm::raw_ostream &OS);

  struct DxilPDBReader {
    llvm::SmallString<32> GetHash();
    uint32_t FindPart(uint32_t part);
  };

}
}
