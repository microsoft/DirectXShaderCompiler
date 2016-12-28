//===-- llvm/MC/MCWinCOFFObjectWriter.h - Win COFF Object Writer *- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCWinCOFFObjectWriter.h                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_MC_MCWINCOFFOBJECTWRITER_H
#define LLVM_MC_MCWINCOFFOBJECTWRITER_H

namespace llvm {
class MCAsmBackend;
class MCFixup;
class MCObjectWriter;
class MCValue;
class raw_ostream;
class raw_pwrite_stream;

  class MCWinCOFFObjectTargetWriter {
    virtual void anchor();
    const unsigned Machine;

  protected:
    MCWinCOFFObjectTargetWriter(unsigned Machine_);

  public:
    virtual ~MCWinCOFFObjectTargetWriter() {}

    unsigned getMachine() const { return Machine; }
    virtual unsigned getRelocType(const MCValue &Target, const MCFixup &Fixup,
                                  bool IsCrossSection,
                                  const MCAsmBackend &MAB) const = 0;
    virtual bool recordRelocation(const MCFixup &) const { return true; }
  };

  /// \brief Construct a new Win COFF writer instance.
  ///
  /// \param MOTW - The target specific WinCOFF writer subclass.
  /// \param OS - The stream to write to.
  /// \returns The constructed object writer.
  MCObjectWriter *createWinCOFFObjectWriter(MCWinCOFFObjectTargetWriter *MOTW,
                                            raw_pwrite_stream &OS);
} // End llvm namespace

#endif
