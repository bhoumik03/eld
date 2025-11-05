//===- x86_64ELFDynamic.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "x86_64ELFDynamic.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/GNULDBackend.h"

using namespace eld;

x86_64ELFDynamic::x86_64ELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig)
    : ELFDynamic(pParent, pConfig) {}

void x86_64ELFDynamic::reserveTargetEntries() {
  reserveOne(llvm::ELF::DT_RELACOUNT);
}

void x86_64ELFDynamic::applyTargetEntries() {
  uint32_t relaCount = 0;
  for (auto &R : m_Backend.getRelaDyn()->getRelocations()) {
    if (R->type() == llvm::ELF::R_X86_64_RELATIVE)
      relaCount++;
  }
  applyOne(llvm::ELF::DT_RELACOUNT, relaCount);
}