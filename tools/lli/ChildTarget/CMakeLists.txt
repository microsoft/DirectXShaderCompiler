# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS support)

add_llvm_executable(lli-child-target
  ChildTarget.cpp
  ../RemoteTarget.cpp
)

set_target_properties(lli-child-target PROPERTIES FOLDER "Misc")
