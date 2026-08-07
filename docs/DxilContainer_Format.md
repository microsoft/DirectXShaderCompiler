# DXIL Container Format

A **DXIL container** is the on-disk/blob format produced by DXC for a compiled
shader or library. It is a small archive of independently-typed **parts**,
wrapping the LLVM bitcode (the actual DXIL program) together with reflection
metadata, signatures, validation data, hashes, debug names, and more.

The format is intentionally backward-compatible with the legacy **DXBC**
container (its header FourCC is literally `'DXBC'`) so existing tooling can walk
the part table, while the individual parts carry DXIL-specific data.

This document describes:

- [What the container is and how it is used](#overview)
- [The container-level structure](#container-level-structure)
- [Alignment, versioning, and hashing](#alignment-versioning-and-hashing)
- [The part catalog](#part-catalog)
- [Part encodings in detail](#part-encodings)
- [Reading a container](#reading-a-container)
- [Appendix: constants](#appendix-constants)

Primary sources for this description:

- [include/dxc/DxilContainer/DxilContainer.h](../include/dxc/DxilContainer/DxilContainer.h) — all container/part header structs and helpers
- [include/dxc/DxilContainer/DxilPipelineStateValidation.h](../include/dxc/DxilContainer/DxilPipelineStateValidation.h) — the PSV0 part
- [lib/DxilContainer/DxilContainerAssembler.cpp](../lib/DxilContainer/DxilContainerAssembler.cpp) — the writer that assembles all parts
- [docs/RDAT_Format.md](RDAT_Format.md) — the RDAT part, documented separately

---

## Overview

### What it is

A DXIL container is a flat, position-independent blob:

- A **container header** (`DxilContainerHeader`) with a FourCC, a hash, a
  version, the total size, and a part count.
- A **part offset table**: one `uint32_t` per part, each an absolute offset
  (from the start of the container) to that part's header.
- A sequence of **parts**, each a `DxilPartHeader` (FourCC + size) followed by
  that part's payload.

Every part is self-describing (FourCC + size), so consumers can skip parts they
do not understand, and new part types can be added without breaking old readers.

### How it is used

- **Runtime/driver**: reads the `DXIL` program part to get the bitcode, and
  `PSV0`/`RDAT` for pipeline validation and reflection.
- **Reflection APIs** (`IDxcContainerReflection`, `ID3D12ShaderReflection`):
  read `STAT`/`RDAT`/signature parts.
- **Debuggers/PIX**: use `ILDB` (debug bitcode), `ILDN` (debug name/PDB
  reference), `SRCI`/`PDBI` (sources & compile args), `HASH`.
- **Validation**: `dxil.dll` validates the container's parts for consistency.
- **Tooling** (`dxa`, `dxc -dumpbin`): extract or print individual parts.

### Which parts appear

The set of parts depends on the shader model and compile flags. From
`SerializeDxilContainerForModule`, the high-level rules are:

- **Feature info** (`SFI0`) is always written first.
- **Non-library** targets get signature parts (`ISG1`, `OSG1`, and `PSG1` if a
  patch-constant/primitive signature exists) and `PSV0`; a `RTS0` root
  signature part if one is present.
- **Library** targets get `VERS` (validator ≥ 1.8) and `RDAT` instead of
  `PSV0`/signatures.
- Debug/reflection/hash parts (`ILDB`, `ILDN`, `HASH`, `STAT`, `SRCI`) are
  gated by flags and validator version.
- The `DXIL` program part is always written (near the end).
- `PRIV` private data, if supplied, is written last (its size is not
  guaranteed aligned).

---

## Container-level structure

```
+---------------------------------------------------------------+
| DxilContainerHeader                                            |
|   uint32_t          HeaderFourCC     = 'DXBC'                 |
|   DxilContainerHash Hash             (16 bytes)               |
|   DxilContainerVersion Version       { uint16 Major, Minor }  |
|   uint32_t          ContainerSizeInBytes                     |
|   uint32_t          PartCount                                |
+---------------------------------------------------------------+
| uint32_t PartOffset[PartCount]   (absolute, from container start) |
+---------------------------------------------------------------+
| Part 0:  DxilPartHeader { uint32 PartFourCC, uint32 PartSize }|
|          uint8_t PartData[PartSize]                          |
+---------------------------------------------------------------+
| Part 1:  DxilPartHeader ...                                   |
| ...                                                           |
+---------------------------------------------------------------+
```

All structures in `DxilContainer.h` are declared under `#pragma pack(push, 1)`,
so there is **no implicit padding** inside these headers; any alignment is
explicit (see below). All integers are little-endian.

### `DxilContainerHeader`

| Offset | Type                   | Field                 | Description                                        |
|-------:|------------------------|-----------------------|----------------------------------------------------|
| 0      | `uint32_t`             | `HeaderFourCC`        | `DFCC_Container` = `'DXBC'` (back-compat with DXBC).|
| 4      | `uint8_t[16]`          | `Hash.Digest`         | Container hash (see hashing below).                |
| 20     | `uint16_t`             | `Version.Major`       | Container major version (currently `1`).           |
| 22     | `uint16_t`             | `Version.Minor`       | Container minor version (currently `0`).           |
| 24     | `uint32_t`             | `ContainerSizeInBytes`| Total container size from start of this header.    |
| 28     | `uint32_t`             | `PartCount`           | Number of parts.                                   |
| 32     | `uint32_t[PartCount]`  | (part offsets)        | Absolute offset of each `DxilPartHeader`.          |

`DxilContainerMaxSize` = `0x80000000` bounds the total size.

### `DxilPartHeader`

| Offset | Type       | Field         | Description                                  |
|-------:|------------|---------------|----------------------------------------------|
| 0      | `uint32_t` | `PartFourCC`  | Part type FourCC (`DFCC_*`).                 |
| 4      | `uint32_t` | `PartSize`    | Size of `PartData`, **not** including header.|
| 8      | `uint8_t[PartSize]` | (data) | Part payload.                             |

The offset table is written after the container header, and each part follows
the previous one contiguously:

```
offset[0]   = sizeof(DxilContainerHeader) + PartCount * 4
offset[i+1] = offset[i] + sizeof(DxilPartHeader) + parts[i].PartSize
```

### Access helpers

`DxilContainer.h` provides inline helpers used throughout the codebase:

- `GetDxilContainerPart(header, index)` — part header by index (via offset table).
- `GetDxilPartData(part)` — pointer to a part's payload (`part + 1`).
- `GetDxilPartByType(header, fourCC)` — find a part by FourCC.
- `GetDxilProgramHeader(header, fourCC)` — the `DxilProgramHeader` of a program part.
- `IsDxilContainerLike(ptr, length)` / `IsValidDxilContainer(...)` — validation.
- `DxilPartIterator`, `DxilPartIsType` — iterate/filter parts.

---

## Alignment, versioning, and hashing

### Alignment

Parts are generally sized to a 4-byte boundary using `PSVALIGN4(x) = (x+3) & ~3`.
Writers pad payloads with zero bytes to reach the aligned size. The main
exception is the trailing `PRIV` private-data part, whose size is caller-defined
and not guaranteed aligned — which is exactly why it is written **last**.

Whether the container writer aligns parts depends on validator version:
`bUnaligned = (ValVer < 1.7)`. Newer containers are aligned.

> **Why the writer's output must be byte-exact (validator behavior).**
> Historically, the DXIL validator verified several container parts not by
> checking their *contents* semantically, but by **regenerating the part bytes
> from the module and running a `memcmp`** against the bytes produced by the
> compiler. This is implemented by `VerifyBlobPartMatches` in
> [lib/DxilValidation/DxilContainerValidation.cpp](../lib/DxilValidation/DxilContainerValidation.cpp),
> which reserves a buffer, calls the part's `DxilPartWriter::write()` to
> re-emit the part, and compares the result to the stored blob:
>
> ```cpp
> pWriter->write(pOutputStream);
> if (memcmp(pData, pOutputStream->GetPtr(), Size)) {
>   ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {pName});
>   return;
> }
> ```
>
> Because this is a raw byte compare, the compiler's serialization — including
> field ordering, reserved/zero fields, and any **alignment padding** — has to
> be *bit-for-bit identical* to what the validator's writer produces. This is a
> key reason the alignment/padding behavior above is tied to the validator
> version: a container built for validator version *N* must match the byte
> layout that version *N*'s writers expect.
>
> **Has this changed?** Partly. In the current validator this
> regenerate-and-`memcmp` approach is *still used* for:
> - program signatures (`ISG1`/`OSG1`/`PSG1`) via `VerifySignatureMatches`,
> - the feature-info part (`SFI0`) via `VerifyFeatureInfoMatches`, and
> - the runtime-data part (`RDAT`) via `VerifyRDATMatches`.
>
> For these parts the writer output must still be byte-exact. However, PSV0
> validation was **changed** to a semantic, field-by-field comparison. The
> older `VerifyPSVMatches` regenerated the whole PSV0 blob and `memcmp`'d it;
> the current implementation uses `PSVContentVerifier`, which decodes both the
> stored part and the module's expected values and compares them field by field
> (emitting a human-readable `ContainerContentMatches` diff on mismatch)
> instead of requiring an identical byte image. This change landed in
> DXC in late 2024 (PRs #6859 and #6924), so validators built before then
> validate PSV0 with the strict byte-for-byte `memcmp`, while newer validators
> tolerate byte-level differences (for example padding) as long as the decoded
> content agrees.

### Container vs. program vs. part versions

There are several independent version numbers:

- **Container version** — `DxilContainerHeader.Version` (major/minor, currently
  1.0).
- **Program version** — inside `DxilProgramHeader.ProgramVersion`, encodes the
  shader kind and shader-model version:

  ```cpp
  ProgramVersion = (ShaderKind << 16) | (Major << 4) | Minor;
  // decode:
  ShaderKind = (ProgramVersion >> 16) & 0xFFFF;
  Major      = (ProgramVersion >> 4)  & 0xF;
  Minor      =  ProgramVersion        & 0xF;
  ```

- **Validator version** — governs which parts and which part-internal versions
  are emitted (e.g. `HASH` needs ≥ 1.5, `VERS` needs ≥ 1.8, unaligned parts for
  < 1.7).
- **Per-part versions** — several parts version *additively by size* (PSV0,
  RDAT records), so a reader infers the version from the recorded size/stride.

### Hashing

`DxilContainerHeader.Hash` is a 16-byte digest. Separately, the `HASH` part
(`DxilShaderHash`) stores a `Flags` field plus a 16-byte MD5 digest computed
over either the final program bitcode or the source-inclusive bitcode
(`DxilShaderHashFlags::IncludesSource` when `-Zss`/`DebugNameDependOnSource`).
A sentinel `PreviewByPassHash` (all `2`s) is used in some preview scenarios.

---

## Part catalog

| FourCC | `DxilFourCC`                    | Contents                                                        |
|--------|---------------------------------|----------------------------------------------------------------|
| `DXBC` | `DFCC_Container`                | Container header FourCC (not a part).                          |
| `RDEF` | `DFCC_ResourceDef`              | Legacy DXBC resource-definition reflection.                    |
| `ISG1` | `DFCC_InputSignature`           | Input signature (`DxilProgramSignature`).                     |
| `OSG1` | `DFCC_OutputSignature`          | Output signature.                                             |
| `PSG1` | `DFCC_PatchConstantSignature`   | Patch-constant / primitive signature.                        |
| `STAT` | `DFCC_ShaderStatistics`         | Reflection data (program-header-wrapped reflection bitcode).  |
| `ILDB` | `DFCC_ShaderDebugInfoDXIL`      | DXIL bitcode **with** debug info.                            |
| `ILDN` | `DFCC_ShaderDebugName`          | Debug name / PDB file reference (`DxilShaderDebugName`).      |
| `SFI0` | `DFCC_FeatureInfo`              | 64-bit shader feature flags (`DxilShaderFeatureInfo`).        |
| `PRIV` | `DFCC_PrivateData`              | Arbitrary caller-provided private data.                      |
| `RTS0` | `DFCC_RootSignature`            | Serialized root signature.                                    |
| `DXIL` | `DFCC_DXIL`                     | The DXIL program (LLVM bitcode via `DxilProgramHeader`).      |
| `PSV0` | `DFCC_PipelineStateValidation`  | Pipeline State Validation data.                              |
| `RDAT` | `DFCC_RuntimeData`              | Runtime Data reflection (see [RDAT doc](RDAT_Format.md)).     |
| `HASH` | `DFCC_ShaderHash`               | Shader hash (`DxilShaderHash`).                             |
| `SRCI` | `DFCC_ShaderSourceInfo`         | Source names, contents, and compile args.                    |
| `PDBI` | `DFCC_ShaderPDBInfo`            | PDB info (RDAT-encoded; replaces `SRCI` in the PDB).         |
| `VERS` | `DFCC_CompilerVersion`          | Compiler version info (`DxilCompilerVersion`).               |

The FourCC value is computed little-endian by `DXIL_FOURCC(c0,c1,c2,c3) =
c0 | c1<<8 | c2<<16 | c3<<24`, i.e. the characters read in order in a hex dump.

---

## Part encodings

### `DXIL` / `ILDB` — the DXIL program

Both the program part (`DXIL`) and the debug-info program part (`ILDB`) share
the same layout: a `DxilProgramHeader` followed by LLVM bitcode.

`DxilProgramHeader`:

| Offset | Type               | Field           | Description                                     |
|-------:|--------------------|-----------------|-------------------------------------------------|
| 0      | `uint32_t`         | `ProgramVersion`| Shader kind + SM version (see encoding above).  |
| 4      | `uint32_t`         | `SizeInUint32`  | Size in `uint32_t` units, including this header. |
| 8      | `DxilBitcodeHeader`| `BitcodeHeader` | Bitcode-specific sub-header.                    |

`DxilBitcodeHeader`:

| Offset | Type       | Field           | Description                                    |
|-------:|------------|-----------------|------------------------------------------------|
| 0      | `uint32_t` | `DxilMagic`     | `'DXIL'` = `0x4C495844`.                        |
| 4      | `uint32_t` | `DxilVersion`   | DXIL version.                                  |
| 8      | `uint32_t` | `BitcodeOffset` | Offset to LLVM bitcode from start of this header.|
| 12     | `uint32_t` | `BitcodeSize`   | Size of the LLVM bitcode.                      |

The bitcode itself is standard LLVM bitcode encoding the DXIL module. The
program payload is zero-padded so the part size is a multiple of 4 bytes.
`ILDB` carries the same module *before* debug info is stripped, so it is only
present when debug info is requested.

### `ISG1` / `OSG1` / `PSG1` — signatures

Each signature part starts with a `DxilProgramSignature` header and an array of
`DxilProgramSignatureElement`, with semantic-name strings stored after the
elements and referenced by offset.

`DxilProgramSignature`:

| Offset | Type       | Field        | Description                                        |
|-------:|------------|--------------|----------------------------------------------------|
| 0      | `uint32_t` | `ParamCount` | Number of signature elements.                      |
| 4      | `uint32_t` | `ParamOffset`| Offset (from start of this header) to the element array. |

`DxilProgramSignatureElement` (exactly `0x20` = 32 bytes, static-asserted):

| Offset | Type                        | Field          | Description                                                  |
|-------:|-----------------------------|----------------|--------------------------------------------------------------|
| 0      | `uint32_t`                  | `Stream`       | Stream index (non-decreasing order).                        |
| 4      | `uint32_t`                  | `SemanticName` | Offset to `LPCSTR` from start of `DxilProgramSignature`.    |
| 8      | `uint32_t`                  | `SemanticIndex`| Semantic index.                                             |
| 12     | `DxilProgramSigSemantic`    | `SystemValue`  | System-value semantic (`uint32_t` enum).                    |
| 16     | `DxilProgramSigCompType`    | `CompType`     | Component type (`uint32_t` enum).                           |
| 20     | `uint32_t`                  | `Register`     | Register (row) index.                                       |
| 24     | `uint8_t`                   | `Mask`         | Column allocation mask.                                     |
| 25     | `uint8_t`                   | `NeverWrites_Mask` / `AlwaysReads_Mask` | Union; write-never (output) or read-always (input) mask. |
| 26     | `uint16_t`                  | `Pad`          | Padding to align.                                           |
| 28     | `DxilProgramSigMinPrecision`| `MinPrecision` | Minimum precision (`uint32_t` enum).                        |

Enums:
- `DxilProgramSigSemantic` mirrors `D3D_NAME` (Position, ClipDistance, Target,
  Depth, Barycentrics, ShadingRate, …).
- `DxilProgramSigCompType` (Unknown/UInt32/SInt32/Float32/…/Float64).
- `DxilProgramSigMinPrecision` (Default/Float16/Float2_8/SInt16/UInt16/Any16/Any10).

### `PSV0` — Pipeline State Validation

The PSV0 part is a size-versioned, self-describing structure used by the D3D12
runtime to validate pipeline state without parsing bitcode. It is written/read
by `DxilPipelineStateValidation::ReadOrWrite`. Every fixed-size sub-structure is
preceded by a `uint32_t` recording its size, so newer (larger) versions are
readable by older code, which just ignores trailing bytes.

Top-level layout, in the order the blocks appear in the blob (from the header
comment in
[DxilPipelineStateValidation.h, lines 899–943](https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/DxilContainer/DxilPipelineStateValidation.h#L899-L943)):

| # | Block | Type / size | Present when | Description |
|--:|-------|-------------|--------------|-------------|
| 1  | `PSVRuntimeInfo_size` | `uint32_t` | always | Size of the following record; selects the runtime-info version *N*. |
| 2  | `PSVRuntimeInfoN` | `PSVRuntimeInfo_size` bytes | always | Fixed per-stage runtime info (see the version table below). |
| 3  | `ResourceCount` | `uint32_t` | always | Number of resource-binding records. |
| 4  | `PSVResourceBindInfo_size` | `uint32_t` | `ResourceCount > 0` | Size of each bind-info record; selects its version. |
| 5  | `PSVResourceBindInfoN` × `ResourceCount` | array | `ResourceCount > 0` | Resource-binding records. |
| 6  | `StringTableSize` | `uint32_t` (dword-aligned) | version ≥ 1 | Size of the string pool in bytes. |
| 7  | String table | `StringTableSize` bytes, null-terminated | version ≥ 1 and `StringTableSize > 0` | Semantic-name pool. |
| 8  | `SemanticIndexTableEntries` | `uint32_t` | version ≥ 1 | Number of dwords in the semantic-index table. |
| 9  | Semantic-index table | `uint32_t` × `SemanticIndexTableEntries` | `SemanticIndexTableEntries > 0` | Semantic-index values. |
| 10 | `PSVSignatureElement_size` | `uint32_t` | any of `SigInputElements` / `SigOutputElements` / `SigPatchConstOrPrimElements` | Size of each signature-element record. |
| 11 | Input signature elements | `PSVSignatureElementN` × `SigInputElements` | as row 10 | Input signature. |
| 12 | Output signature elements | `PSVSignatureElementN` × `SigOutputElements` | as row 10 | Output signature. |
| 13 | Patch-const/prim elements | `PSVSignatureElementN` × `SigPatchConstOrPrimElements` | as row 10 | Patch-constant / primitive signature. |
| 14 | ViewID→output masks | per stream *i* (0–3): `uint32_t` × `MaskDwords(SigOutputVectors[i])` | `UsesViewID` and `SigOutputVectors[i] != 0` | Outputs affected by ViewID, as a bitmask. |
| 15 | ViewID→patch-const masks | `uint32_t` × `MaskDwords(SigPatchConstOrPrimVectors)` | `UsesViewID` and HS and `SigPatchConstOrPrimVectors != 0` | Patch-const outputs affected by ViewID. |
| 16 | Input→output tables | per stream *i* (0–3): `uint32_t` × `InputOutputTableDwords(SigInputVectors, SigOutputVectors[i])` | `SigInputVectors` and `SigOutputVectors[i] != 0` | Outputs affected by inputs, as a table of bitmasks. |
| 17 | Input→patch-const table | `uint32_t` × `InputOutputTableDwords(SigInputVectors, SigPatchConstOrPrimVectors)` | HS and `SigPatchConstOrPrimVectors` and `SigInputVectors` | Patch-const outputs affected by inputs. |
| 18 | Patch-const→output table | `uint32_t` × `InputOutputTableDwords(SigPatchConstOrPrimVectors, SigOutputVectors[0])` | DS and `SigOutputVectors[0]` and `SigPatchConstOrPrimVectors` | Outputs affected by patch-constant inputs. |

Where the dword-count helpers are:

- `MaskDwords(vectors) = (vectors + 7) >> 3` (one bit per component, 4
  components per vector).
- `InputOutputTableDwords(in, out) = MaskDwords(out) * in * 4`.

**`PSVRuntimeInfo0`** (base, `MAX_PSV_VERSION = 4`) begins with a union of
per-stage info (`VSInfo`/`HSInfo`/`DSInfo`/`GSInfo`/`PSInfo`/`MSInfo`/`ASInfo`)
followed by `MinimumExpectedWaveLaneCount`/`MaximumExpectedWaveLaneCount`.
Successive versions append fields:

- **`PSVRuntimeInfo1`**: `ShaderStage` (`PSVShaderKind`), `UsesViewID`, a union
  (`MaxVertexCount` for GS / `SigPatchConstOrPrimVectors` / `MSInfo1`), signature
  element counts (`SigInputElements`, `SigOutputElements`,
  `SigPatchConstOrPrimElements`), and packed vector counts (`SigInputVectors`,
  `SigOutputVectors[4]` for GS streams).
- **`PSVRuntimeInfo2`**: `NumThreadsX/Y/Z`.
- **`PSVRuntimeInfo3`**: `EntryFunctionName` (offset into the string table).
- **`PSVRuntimeInfo4`**: `NumBytesGroupSharedMemory`.

**`PSVResourceBindInfo0`**: `ResType` (`PSVResourceType`), `Space`, `LowerBound`,
`UpperBound`. **`PSVResourceBindInfo1`** adds `ResKind` (`PSVResourceKind`) and
`ResFlags` (`PSVResourceFlag`, e.g. `UsedByAtomic64`).

**`PSVSignatureElement0`** (referenced via the string/semantic-index tables):

| Offset | Type       | Field                 | Description                                             |
|-------:|------------|-----------------------|---------------------------------------------------------|
| 0      | `uint32_t` | `SemanticName`        | Offset into PSV string table.                          |
| 4      | `uint32_t` | `SemanticIndexes`     | Offset into semantic-index table; count == `Rows`.     |
| 8      | `uint8_t`  | `Rows`                | Rows occupied.                                          |
| 9      | `uint8_t`  | `StartRow`            | Packing start row (if allocated).                      |
| 10     | `uint8_t`  | `ColsAndStart`        | bits 0:4 `Cols`, 4:6 `StartCol`, bit 6 `Allocated`.    |
| 11     | `uint8_t`  | `SemanticKind`        | `PSVSemanticKind`.                                     |
| 12     | `uint8_t`  | `ComponentType`       | `DxilProgramSigCompType`.                              |
| 13     | `uint8_t`  | `InterpolationMode`   | `DXIL::InterpolationMode`.                             |
| 14     | `uint8_t`  | `DynamicMaskAndStream`| bits 0:4 dynamic-index mask, 4:6 output stream.        |
| 15     | `uint8_t`  | `Reserved`            | Reserved.                                              |

The PSV string table is dword-aligned with a trailing null; the semantic-index
table is an array of `uint32_t`. ViewID and input→output dependency data are
stored as packed component-mask bitfields (see helpers above).

### `SFI0` — feature info

`DxilShaderFeatureInfo` is a single `uint64_t FeatureFlags`. The bit meanings
are the shader feature flags (Doubles, WaveOps, Int64Ops, ViewID, Barycentrics,
Raytracing Tier 1.1, ResourceDescriptorHeapIndexing, etc.), matching the
`DxilFeatureInfo1`/`DxilFeatureInfo2` enumerations documented in the
[RDAT doc](RDAT_Format.md) (there the same 64-bit value is split into two 32-bit
halves).

### `HASH` — shader hash

`DxilShaderHash`:

| Offset | Type          | Field    | Description                                    |
|-------:|---------------|----------|------------------------------------------------|
| 0      | `uint32_t`    | `Flags`  | `DxilShaderHashFlags` (`IncludesSource` = 1).  |
| 4      | `uint8_t[16]` | `Digest` | 16-byte MD5 digest.                            |

Written only when validator version ≥ 1.5.

### `ILDN` — shader debug name

`DxilShaderDebugName` followed by the name bytes:

| Offset | Type       | Field        | Description                                    |
|-------:|------------|--------------|------------------------------------------------|
| 0      | `uint16_t` | `Flags`      | Reserved, must be 0.                           |
| 2      | `uint16_t` | `NameLength` | Debug name length, excluding null terminator.  |
| 4      | `char[NameLength]` | (name) | UTF-8 name.                                |
| …      | `char`     | (null)       | Null terminator.                               |
| …      | `char[0-3]`| (pad)        | Zero padding to a 4-byte boundary.             |

`MinDxilShaderDebugNameSize = sizeof(DxilShaderDebugName) + 4`. This part
typically names the associated `.pdb` (default: `<hash>.pdb`).

### `VERS` — compiler version

`DxilCompilerVersion`:

| Offset | Type       | Field                            | Description                                  |
|-------:|------------|----------------------------------|----------------------------------------------|
| 0      | `uint16_t` | `Major`                          | Compiler major version.                      |
| 2      | `uint16_t` | `Minor`                          | Compiler minor version.                      |
| 4      | `uint32_t` | `VersionFlags`                   | Flags.                                       |
| 8      | `uint32_t` | `CommitCount`                    | Commit count.                                |
| 12     | `uint32_t` | `VersionStringListSizeInBytes`   | Size of the string list that follows.        |
| 16     | `char[...]`| (strings)                        | Up to two null-terminated strings: commit SHA, then custom version string. Zero-padded to 4 bytes. |

Written for library targets when validator version ≥ 1.8 and version info is
available.

### `SRCI` — shader source info

A `DxilSourceInfo` header followed by `SectionCount` sections, each a
`DxilSourceInfoSection` header plus a type-specific, 4-byte-aligned payload.

`DxilSourceInfo`:

| Offset | Type       | Field                | Description                                  |
|-------:|------------|----------------------|----------------------------------------------|
| 0      | `uint32_t` | `AlignedSizeInBytes` | Total size including this header.            |
| 4      | `uint16_t` | `Flags`              | Reserved, 0.                                 |
| 6      | `uint16_t` | `SectionCount`       | Number of sections.                          |

`DxilSourceInfoSection`:

| Offset | Type                        | Field                | Description                              |
|-------:|-----------------------------|----------------------|------------------------------------------|
| 0      | `uint32_t`                  | `AlignedSizeInBytes` | Section size including header + padding.  |
| 4      | `uint16_t`                  | `Flags`              | Reserved, 0.                             |
| 6      | `DxilSourceInfoSectionType` | `Type`               | `SourceContents` / `SourceNames` / `Args`.|

Section payloads:

- **Source names** (`DxilSourceInfo_SourceNames` + `..._SourceNamesEntry[]`):
  `Count`, `EntriesSizeInBytes`, then per-entry `AlignedSizeInBytes`, `Flags`,
  `NameSizeInBytes`, `ContentSizeInBytes`, followed by the UTF-8 name (null
  terminated, padded to 4 bytes).
- **Source contents** (`DxilSourceInfo_SourceContents` +
  `..._SourceContentsEntry[]`): may be **zlib-compressed** (`CompressType`), with
  `EntriesSizeInBytes` (compressed) and `UncompressedEntriesSizeInBytes`; each
  uncompressed entry is `AlignedSizeInBytes`, `Flags`, `ContentSizeInBytes`, then
  the content (null terminated, padded).
- **Args** (`DxilSourceInfo_Args`): `Flags`, `SizeInBytes`, `Count`, then
  `Count` argument pairs encoded as `Name\0Value\0` (e.g. `T\0ps_6_0\0`,
  `D\0MyDefine=1\0`, `Zi\0\0`).

### `PDBI` — shader PDB info

`DxilShaderPDBInfo` header followed by (optionally zlib-compressed) data that is
itself **RDAT-encoded** (see `RDAT_PdbInfoTypes.inl` and the
[RDAT doc](RDAT_Format.md)). It supersedes `SRCI` inside PDB files.

| Offset | Type                              | Field                     | Description                        |
|-------:|-----------------------------------|---------------------------|------------------------------------|
| 0      | `DxilShaderPDBInfoVersion` (`uint16_t`) | `Version`           | `Version_0` currently.             |
| 2      | `DxilShaderPDBInfoCompressionType` (`uint16_t`) | `CompressionType` | `Uncompressed` or `Zlib`.       |
| 4      | `uint32_t`                        | `SizeInBytes`             | Size of the (possibly compressed) data. |
| 8      | `uint32_t`                        | `UncompressedSizeInBytes` | Uncompressed size.                 |

### `RTS0` — root signature

A serialized root signature blob (the same format D3D12 consumes via
`D3D12SerializeRootSignature`). It can be embedded in the container and/or
emitted standalone (wrapped in its own single-part container via
`SerializeDxilContainerForRootSignature`). For non-library targets, DXC copies
the module's serialized root signature into this part.

### `STAT` — shader statistics / reflection

A program part (same `DxilProgramHeader` wrapper as `DXIL`) whose bitcode is a
**reflection-only** clone of the module produced by
`StripAndCreateReflectionStream`. Reflection APIs read this to expose full
reflection for both shaders and libraries. For libraries, an `RDAT` part is
appended alongside `STAT` in the separate reflection stream.

### `PRIV` — private data

Opaque, caller-provided bytes copied verbatim. Written **last** because its size
is not guaranteed to be 4-byte aligned, so it must not perturb the alignment of
following parts (there are none after it).

### `RDEF` — resource definitions (legacy)

The legacy DXBC `RDEF` reflection part is recognized by the FourCC enumeration
for back-compat with DXBC tooling. DXIL reflection is primarily carried by
`STAT`/`RDAT`; see those parts and [RDAT_Format.md](RDAT_Format.md).

---

## Reading a container

A typical consumer:

1. Validate with `IsDxilContainerLike(ptr, length)` (checks the `'DXBC'` FourCC
   and minimum length) and, for stronger guarantees, `IsValidDxilContainer`.
2. Read `PartCount` and the offset table.
3. For each index, `GetDxilContainerPart` returns the `DxilPartHeader`; switch on
   `PartFourCC`, or use `GetDxilPartByType` to jump to a specific part.
4. Use `GetDxilPartData` to access a part's payload, bounded by `PartSize`.
5. For program parts, `GetDxilProgramHeader` yields the `DxilProgramHeader`; the
   bitcode is at `BitcodeHeader.BitcodeOffset` for `BitcodeHeader.BitcodeSize`
   bytes, and `GetVersionShaderType`/`GetVersionMajor`/`GetVersionMinor` decode
   the shader model.

Unknown FourCCs should be skipped — this is what makes the format extensible.

---

## Appendix: constants

| Name                         | Value            | Meaning                                     |
|------------------------------|------------------|---------------------------------------------|
| `DFCC_Container`             | `'DXBC'`         | Container header FourCC.                     |
| `DxilContainerVersionMajor`  | `1`              | Current container major version.            |
| `DxilContainerVersionMinor`  | `0`              | Current container minor version.            |
| `DxilContainerMaxSize`       | `0x80000000`     | Maximum container size.                      |
| `DxilContainerHashSize`      | `16`             | Container/hash digest length in bytes.       |
| `DxilMagicValue`             | `0x4C495844`     | `'DXIL'` bitcode magic.                      |
| `MinDxilShaderDebugNameSize` | `sizeof(DxilShaderDebugName)+4` | Minimum `ILDN` part size.     |
| `MAX_PSV_VERSION`            | `4`              | Highest PSV runtime-info version.            |
| `PSVALIGN4(x)`               | `(x+3)&~3`       | 4-byte alignment used across parts.          |
| `PreviewByPassHash`          | all `2`s         | Sentinel container hash for preview.         |

### Program version encoding

```
ProgramVersion = (ShaderKind << 16) | (Major << 4) | Minor
```

### FourCC encoding

```
DXIL_FOURCC(c0, c1, c2, c3) = c0 | (c1 << 8) | (c2 << 16) | (c3 << 24)
```
