#ifndef LINEARALGEBRATESTDATA_H
#define LINEARALGEBRATESTDATA_H

#include <vector>

#include <Verify.h>

#include "HLSLTestDataTypes.h"

namespace LinearAlgebra {

//
// Input data sets for matrix operations.
// Follows the same InputSet / BEGIN_INPUT_SETS pattern as LongVectorTestData.h.
//

enum class InputSet { Seed, Fill, Identity };

template <typename T> const std::vector<T> &getInputSet(InputSet InputSet) {
  static_assert(sizeof(T) == 0, "No InputSet for this type");
}

#define BEGIN_INPUT_SETS(TYPE)                                                 \
  template <>                                                                  \
  inline const std::vector<TYPE> &getInputSet<TYPE>(InputSet InputSet) {       \
    using T = TYPE;                                                            \
    switch (InputSet) {

#define INPUT_SET(SET, ...)                                                    \
  case SET: {                                                                  \
    static std::vector<T> Data = {__VA_ARGS__};                                \
    return Data;                                                               \
  }

#define END_INPUT_SETS()                                                       \
  default:                                                                     \
    break;                                                                     \
    }                                                                          \
    VERIFY_FAIL("Missing input set");                                          \
    std::abort();                                                              \
    }

using HLSLTestDataTypes::F8E4M3_t;
using HLSLTestDataTypes::F8E5M2_t;
using HLSLTestDataTypes::HLSLHalf_t;
using HLSLTestDataTypes::SNormF16_t;
using HLSLTestDataTypes::SNormF32_t;
using HLSLTestDataTypes::SNormF64_t;
using HLSLTestDataTypes::UNormF16_t;
using HLSLTestDataTypes::UNormF32_t;
using HLSLTestDataTypes::UNormF64_t;

BEGIN_INPUT_SETS(HLSLHalf_t)
INPUT_SET(InputSet::Seed, HLSLHalf_t(1.0f), HLSLHalf_t(2.0f), HLSLHalf_t(3.0f),
          HLSLHalf_t(4.0f), HLSLHalf_t(5.0f), HLSLHalf_t(6.0f),
          HLSLHalf_t(7.0f), HLSLHalf_t(8.0f), HLSLHalf_t(9.0f),
          HLSLHalf_t(10.0f), HLSLHalf_t(11.0f), HLSLHalf_t(12.0f),
          HLSLHalf_t(13.0f), HLSLHalf_t(14.0f))
INPUT_SET(InputSet::Fill, HLSLHalf_t(42.0f))
INPUT_SET(InputSet::Identity, HLSLHalf_t(1.0f))
END_INPUT_SETS()

BEGIN_INPUT_SETS(float)
INPUT_SET(InputSet::Seed, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f,
          10.0f, 11.0f, 12.0f, 13.0f, 14.0f)
INPUT_SET(InputSet::Fill, 42.0f)
INPUT_SET(InputSet::Identity, 1.0f)
END_INPUT_SETS()

BEGIN_INPUT_SETS(double)
INPUT_SET(InputSet::Seed, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
          11.0, 12.0, 13.0, 14.0)
INPUT_SET(InputSet::Fill, 42.0)
INPUT_SET(InputSet::Identity, 1.0)
END_INPUT_SETS()

BEGIN_INPUT_SETS(int32_t)
INPUT_SET(InputSet::Seed, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)
INPUT_SET(InputSet::Fill, 42)
INPUT_SET(InputSet::Identity, 1)
END_INPUT_SETS()

BEGIN_INPUT_SETS(uint32_t)
INPUT_SET(InputSet::Seed, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)
INPUT_SET(InputSet::Fill, 42)
INPUT_SET(InputSet::Identity, 1)
END_INPUT_SETS()

// --- Additional scalar types ---

BEGIN_INPUT_SETS(int8_t)
INPUT_SET(InputSet::Seed, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)
INPUT_SET(InputSet::Fill, 42)
INPUT_SET(InputSet::Identity, 1)
END_INPUT_SETS()

BEGIN_INPUT_SETS(uint8_t)
INPUT_SET(InputSet::Seed, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)
INPUT_SET(InputSet::Fill, 42)
INPUT_SET(InputSet::Identity, 1)
END_INPUT_SETS()

BEGIN_INPUT_SETS(int16_t)
INPUT_SET(InputSet::Seed, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)
INPUT_SET(InputSet::Fill, 42)
INPUT_SET(InputSet::Identity, 1)
END_INPUT_SETS()

BEGIN_INPUT_SETS(uint16_t)
INPUT_SET(InputSet::Seed, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)
INPUT_SET(InputSet::Fill, 42)
INPUT_SET(InputSet::Identity, 1)
END_INPUT_SETS()

BEGIN_INPUT_SETS(int64_t)
INPUT_SET(InputSet::Seed, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)
INPUT_SET(InputSet::Fill, 42)
INPUT_SET(InputSet::Identity, 1)
END_INPUT_SETS()

BEGIN_INPUT_SETS(uint64_t)
INPUT_SET(InputSet::Seed, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)
INPUT_SET(InputSet::Fill, 42)
INPUT_SET(InputSet::Identity, 1)
END_INPUT_SETS()

// --- Normalized types (SNorm [-1,1], UNorm [0,1]) ---

BEGIN_INPUT_SETS(SNormF16_t)
INPUT_SET(InputSet::Seed, SNormF16_t(HLSLHalf_t(-0.9f)),
          SNormF16_t(HLSLHalf_t(-0.7f)), SNormF16_t(HLSLHalf_t(-0.5f)),
          SNormF16_t(HLSLHalf_t(-0.3f)), SNormF16_t(HLSLHalf_t(-0.1f)),
          SNormF16_t(HLSLHalf_t(0.1f)), SNormF16_t(HLSLHalf_t(0.2f)),
          SNormF16_t(HLSLHalf_t(0.3f)), SNormF16_t(HLSLHalf_t(0.4f)),
          SNormF16_t(HLSLHalf_t(0.5f)), SNormF16_t(HLSLHalf_t(0.6f)),
          SNormF16_t(HLSLHalf_t(0.7f)), SNormF16_t(HLSLHalf_t(0.8f)),
          SNormF16_t(HLSLHalf_t(0.9f)))
INPUT_SET(InputSet::Fill, SNormF16_t(HLSLHalf_t(0.5f)))
INPUT_SET(InputSet::Identity, SNormF16_t(HLSLHalf_t(1.0f)))
END_INPUT_SETS()

BEGIN_INPUT_SETS(UNormF16_t)
INPUT_SET(InputSet::Seed, UNormF16_t(HLSLHalf_t(0.05f)),
          UNormF16_t(HLSLHalf_t(0.1f)), UNormF16_t(HLSLHalf_t(0.15f)),
          UNormF16_t(HLSLHalf_t(0.2f)), UNormF16_t(HLSLHalf_t(0.25f)),
          UNormF16_t(HLSLHalf_t(0.3f)), UNormF16_t(HLSLHalf_t(0.35f)),
          UNormF16_t(HLSLHalf_t(0.4f)), UNormF16_t(HLSLHalf_t(0.45f)),
          UNormF16_t(HLSLHalf_t(0.5f)), UNormF16_t(HLSLHalf_t(0.55f)),
          UNormF16_t(HLSLHalf_t(0.6f)), UNormF16_t(HLSLHalf_t(0.7f)),
          UNormF16_t(HLSLHalf_t(0.8f)))
INPUT_SET(InputSet::Fill, UNormF16_t(HLSLHalf_t(0.5f)))
INPUT_SET(InputSet::Identity, UNormF16_t(HLSLHalf_t(1.0f)))
END_INPUT_SETS()

BEGIN_INPUT_SETS(SNormF32_t)
INPUT_SET(InputSet::Seed, SNormF32_t(-0.9f), SNormF32_t(-0.7f),
          SNormF32_t(-0.5f), SNormF32_t(-0.3f), SNormF32_t(-0.1f),
          SNormF32_t(0.1f), SNormF32_t(0.2f), SNormF32_t(0.3f),
          SNormF32_t(0.4f), SNormF32_t(0.5f), SNormF32_t(0.6f),
          SNormF32_t(0.7f), SNormF32_t(0.8f), SNormF32_t(0.9f))
INPUT_SET(InputSet::Fill, SNormF32_t(0.5f))
INPUT_SET(InputSet::Identity, SNormF32_t(1.0f))
END_INPUT_SETS()

BEGIN_INPUT_SETS(UNormF32_t)
INPUT_SET(InputSet::Seed, UNormF32_t(0.05f), UNormF32_t(0.1f),
          UNormF32_t(0.15f), UNormF32_t(0.2f), UNormF32_t(0.25f),
          UNormF32_t(0.3f), UNormF32_t(0.35f), UNormF32_t(0.4f),
          UNormF32_t(0.45f), UNormF32_t(0.5f), UNormF32_t(0.55f),
          UNormF32_t(0.6f), UNormF32_t(0.7f), UNormF32_t(0.8f))
INPUT_SET(InputSet::Fill, UNormF32_t(0.5f))
INPUT_SET(InputSet::Identity, UNormF32_t(1.0f))
END_INPUT_SETS()

BEGIN_INPUT_SETS(SNormF64_t)
INPUT_SET(InputSet::Seed, SNormF64_t(-0.9), SNormF64_t(-0.7), SNormF64_t(-0.5),
          SNormF64_t(-0.3), SNormF64_t(-0.1), SNormF64_t(0.1), SNormF64_t(0.2),
          SNormF64_t(0.3), SNormF64_t(0.4), SNormF64_t(0.5), SNormF64_t(0.6),
          SNormF64_t(0.7), SNormF64_t(0.8), SNormF64_t(0.9))
INPUT_SET(InputSet::Fill, SNormF64_t(0.5))
INPUT_SET(InputSet::Identity, SNormF64_t(1.0))
END_INPUT_SETS()

BEGIN_INPUT_SETS(UNormF64_t)
INPUT_SET(InputSet::Seed, UNormF64_t(0.05), UNormF64_t(0.1), UNormF64_t(0.15),
          UNormF64_t(0.2), UNormF64_t(0.25), UNormF64_t(0.3), UNormF64_t(0.35),
          UNormF64_t(0.4), UNormF64_t(0.45), UNormF64_t(0.5), UNormF64_t(0.55),
          UNormF64_t(0.6), UNormF64_t(0.7), UNormF64_t(0.8))
INPUT_SET(InputSet::Fill, UNormF64_t(0.5))
INPUT_SET(InputSet::Identity, UNormF64_t(1.0))
END_INPUT_SETS()

// --- FP8 types (packed 4 elements per scalar in HLSL) ---

BEGIN_INPUT_SETS(F8E4M3_t)
INPUT_SET(InputSet::Seed, F8E4M3_t(1.0f), F8E4M3_t(1.5f), F8E4M3_t(2.0f),
          F8E4M3_t(2.5f), F8E4M3_t(3.0f), F8E4M3_t(4.0f), F8E4M3_t(5.0f),
          F8E4M3_t(6.0f), F8E4M3_t(7.0f), F8E4M3_t(8.0f), F8E4M3_t(0.5f),
          F8E4M3_t(0.25f), F8E4M3_t(0.75f), F8E4M3_t(10.0f))
INPUT_SET(InputSet::Fill, F8E4M3_t(2.0f))
INPUT_SET(InputSet::Identity, F8E4M3_t(1.0f))
END_INPUT_SETS()

BEGIN_INPUT_SETS(F8E5M2_t)
INPUT_SET(InputSet::Seed, F8E5M2_t(1.0f), F8E5M2_t(1.5f), F8E5M2_t(2.0f),
          F8E5M2_t(3.0f), F8E5M2_t(4.0f), F8E5M2_t(5.0f), F8E5M2_t(6.0f),
          F8E5M2_t(7.0f), F8E5M2_t(8.0f), F8E5M2_t(0.5f), F8E5M2_t(0.25f),
          F8E5M2_t(0.75f), F8E5M2_t(10.0f), F8E5M2_t(12.0f))
INPUT_SET(InputSet::Fill, F8E5M2_t(2.0f))
INPUT_SET(InputSet::Identity, F8E5M2_t(1.0f))
END_INPUT_SETS()

#undef BEGIN_INPUT_SETS
#undef INPUT_SET
#undef END_INPUT_SETS

} // namespace LinearAlgebra

#endif // LINEARALGEBRATESTDATA_H
