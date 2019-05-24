#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"

namespace llvm {
  class raw_ostream;
}

struct IDxcBlob;
struct IStream;

namespace hlsl {
namespace pdb {

  HRESULT LoadDataFromStream(IMalloc *pMalloc, IStream *pIStream, IDxcBlob **pOutContainer);
  void WriteDxilPDB(llvm::ArrayRef<char> Data, llvm::SmallString<32> Hash, llvm::raw_ostream &OS);

}
}
