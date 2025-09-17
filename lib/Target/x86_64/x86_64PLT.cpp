//===- x86_64PLT.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "x86_64PLT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"
#include "eld/Target/Relocator.h"

using namespace eld;

// PLT0
x86_64PLT0 *x86_64PLT0::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                               ResolveInfo *R, bool BindNow) {

  x86_64PLT0 *P = make<x86_64PLT0>(G, I, O, R, 16, 16);
  O->addFragmentAndUpdateSize(P);

  llvm::outs() << "PLT0 created at: " << P << "\n";

  std::string name = "__gotplt0__";
  LDSymbol *symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local, 8, 0, make<FragmentRef>(*G, 0), ResolveInfo::Default,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);

  Relocation *r1 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                      make<FragmentRef>(*P, 2), 4);
  r1->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r1);

  Relocation *r2 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                      make<FragmentRef>(*P, 8), 12);
  r2->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r2);

  return P;
}

x86_64PLTN *x86_64PLTN::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                               ResolveInfo *R, bool BindNow) {
  x86_64PLTN *P = make<x86_64PLTN>(G, I, O, R, 16, 16);

  // std::string name = "__gotpltn_for_" + std::string(R->name());
  // LDSymbol *symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
  //     O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
  //     ResolveInfo::Local, 8, 0, make<FragmentRef>(*G, 0),
  //     ResolveInfo::Default, true);
  // symbol->setShouldIgnore(false);

  // Relocation *r1 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
  //                                     make<FragmentRef>(*P, 2), 0);
  // r1->setSymInfo(symbol->resolveInfo());
  // O->addRelocation(r1);

  // if (!BindNow) {
  //   if (O->getFragmentList().size() > 0) {
  //     Fragment *PLT0 = *(O->getFragmentList().begin());

  //     std::string plt0_name = "__plt0__";
  //     LDSymbol *plt0_symbol = I.addSymbol<IRBuilder::Force,
  //     IRBuilder::Resolve>(
  //         O->getInputFile(), plt0_name, ResolveInfo::NoType,
  //         ResolveInfo::Define, ResolveInfo::Local, 16, 0,
  //         make<FragmentRef>(*PLT0, 0), ResolveInfo::Default, true);
  //     plt0_symbol->setShouldIgnore(false);

  //     Relocation *r3 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
  //                                         make<FragmentRef>(*P, 11), 0);
  //     r3->setSymInfo(plt0_symbol->resolveInfo());
  //     O->addRelocation(r3);
  //   }
  // }

  return P;
}
