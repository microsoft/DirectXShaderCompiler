//===- IntrinsicsBPF.td - Defines BPF intrinsics -----------*- tablegen -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// IntrinsicsBPF.td                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines all of the BPF-specific intrinsics.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
// Specialized loads from packet                                             //

let TargetPrefix = "bpf" in {  // All intrinsics start with "llvm.bpf."
  def int_bpf_load_byte : GCCBuiltin<"__builtin_bpf_load_byte">,
              Intrinsic<[llvm_i64_ty], [llvm_ptr_ty, llvm_i64_ty], [IntrReadMem]>;
  def int_bpf_load_half : GCCBuiltin<"__builtin_bpf_load_half">,
              Intrinsic<[llvm_i64_ty], [llvm_ptr_ty, llvm_i64_ty], [IntrReadMem]>;
  def int_bpf_load_word : GCCBuiltin<"__builtin_bpf_load_word">,
              Intrinsic<[llvm_i64_ty], [llvm_ptr_ty, llvm_i64_ty], [IntrReadMem]>;
  def int_bpf_pseudo : GCCBuiltin<"__builtin_bpf_pseudo">,
              Intrinsic<[llvm_i64_ty], [llvm_i64_ty, llvm_i64_ty]>;
}
