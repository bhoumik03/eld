//===- x86_64PLT.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "x86_64PLT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"

using namespace eld;

// // PLT0
// x86_64PLT0 *x86_64PLT0::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection
// *O,
//                                ResolveInfo *R, bool BindNow) {
//   // No need of PLT0 when binding now.
//   if (BindNow)
//     return nullptr;
//   x86_64PLT0 *P = make<x86_64PLT0>(G, I, O, R, 16, 16);
//   O->addFragmentAndUpdateSize(P);

//   // Create relocations for PLT0 template:
//   // PLT0 template: pushq GOTPLT+8(%rip); jmp *GOTPLT+16(%rip)
//   Relocation *r1 = nullptr;
//   Relocation *r2 = nullptr;

//   // Relocation for pushq GOTPLT+8(%rip) - offset 2 in template
//   r1 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
//                           make<FragmentRef>(*P, 2), 8); // GOT+8 offset
//   llvm::outs() << "PLT0 relocations created\n";
//   r1->setTargetRef(make<FragmentRef>(*G, 8)); // Point to GOTPLT[1]
//   O->addRelocation(r1);

//   // Relocation for jmp *GOTPLT+16(%rip) - offset 8 in template
//   r2 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
//                           make<FragmentRef>(*P, 8), 16); // GOT+16 offset
//   r2->setTargetRef(make<FragmentRef>(*G, 16));           // Point to
//   GOTPLT[2] O->addRelocation(r2);

//   return P;
// }

// PLT0
x86_64PLT0 *x86_64PLT0::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                               ResolveInfo *R, bool BindNow) {
  // No need of PLT0 when binding now.
  if (BindNow)
    return nullptr;

  // Create PLT0 fragment - constructor handles fragment addition
  x86_64PLT0 *P = make<x86_64PLT0>(G, I, O, R, 16, 16);
  if (O)
    O->addFragmentAndUpdateSize(P);

  // Symbol for GOTPLT+8 (resolver link_map parameter)
  std::string gotplt8_name = "__gotplt8__";
  LDSymbol *gotplt8_symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), gotplt8_name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local, 8, 0, make<FragmentRef>(*G, 8), ResolveInfo::Default,
      true);
  gotplt8_symbol->setShouldIgnore(false);

  // Symbol for GOTPLT+16 (resolver function pointer)
  std::string gotplt16_name = "__gotplt16__";
  LDSymbol *gotplt16_symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), gotplt16_name, ResolveInfo::NoType,
      ResolveInfo::Define, ResolveInfo::Local, 8, 0, make<FragmentRef>(*G, 16),
      ResolveInfo::Default, true);
  gotplt16_symbol->setShouldIgnore(false);

  // Create relocations for PLT0 template:
  // PLT0 template: pushq GOTPLT+8(%rip); jmp *GOTPLT+16(%rip)

  // Relocation for pushq GOTPLT+8(%rip) - offset 2 in template
  Relocation *r1 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                      make<FragmentRef>(*P, 2), -4);
  r1->setSymInfo(gotplt8_symbol->resolveInfo());
  O->addRelocation(r1);

  // Relocation for jmp *GOTPLT+16(%rip) - offset 8 in template
  Relocation *r2 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                      make<FragmentRef>(*P, 8), -4);
  r2->setSymInfo(gotplt16_symbol->resolveInfo());
  O->addRelocation(r2);

  return P;
}

// // PLTN
// x86_64PLTN *x86_64PLTN::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection
// *O,
//                                ResolveInfo *R, bool BindNow) {
//   x86_64PLTN *P = make<x86_64PLTN>(G, I, O, R, 16, 16);
//   O->addFragmentAndUpdateSize(P);

//   // Create relocation for jmpq *got(%rip) instruction
//   // PLTN template: jmpq *got(%rip); pushq $index; jmpq PLT0
//   Relocation *r1 =
//       Relocation::Create(llvm::ELF::R_X86_64_PC32, 32, make<FragmentRef>(*P,
//       2),
//                          0);                  // jmpq offset 2
//   r1->setTargetRef(make<FragmentRef>(*G, 0)); // Point to this GOT entry
//   O->addRelocation(r1);

//   // For non-immediate binding, create PLT0 reference for jmpq PLT0
//   if (!BindNow) {
//     Fragment *PLT0 = *(O->getFragmentList().begin());
//     Relocation *r2 =
//         Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
//                            make<FragmentRef>(*P, 12), 0); // jmpq PLT0 offset
//                            12
//     r2->setTargetRef(make<FragmentRef>(*PLT0, 0));        // Point to PLT0
//     start O->addRelocation(r2);
//   }

//   return P;
// }

// PLTN
x86_64PLTN *x86_64PLTN::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                               ResolveInfo *R, bool BindNow) {
  // Create PLTN fragment
  x86_64PLTN *P = make<x86_64PLTN>(G, I, O, R, 16, 16);
  if (O)
    O->addFragmentAndUpdateSize(P);

  // Create symbol for this function's GOT entry (following AArch64 pattern)
  std::string gotpltn_name = "__gotpltn_for_" + std::string(R->name());
  LDSymbol *gotpltn_symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), gotpltn_name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local, 8, 0, make<FragmentRef>(*G, 0), ResolveInfo::Default,
      true);
  gotpltn_symbol->setShouldIgnore(false);

  // Create relocation for jmpq *got(%rip) instruction (offset 2)
  Relocation *r1 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                      make<FragmentRef>(*P, 2), -4);
  r1->setSymInfo(gotpltn_symbol->resolveInfo());
  O->addRelocation(r1);

  // Calculate relocation index for this PLT entry
  uint32_t rela_index = 0;
  if (O->getFragmentList().size() > 1) {
    // Count PLTN entries (excluding PLT0)
    rela_index = O->getFragmentList().size() - 2; // -1 for PLT0, -1 for 0-based
  }

  // Create a symbol for the relocation index value
  std::string index_name = "__rela_index_" + std::string(R->name());
  LDSymbol *index_symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), index_name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Absolute, 4, rela_index, FragmentRef::null(),
      ResolveInfo::Default, true);
  index_symbol->setShouldIgnore(false);

  // Create relocation for pushq <relocation index> instruction (offset 7)
  Relocation *r2 = Relocation::Create(llvm::ELF::R_X86_64_32, 32,
                                      make<FragmentRef>(*P, 7), 0);
  r2->setSymInfo(index_symbol->resolveInfo());
  O->addRelocation(r2);

  // For non-immediate binding, create PLT0 reference for jmpq PLT0 (offset 12)
  if (!BindNow && O->getFragmentList().size() > 0) {
    Fragment *PLT0 = *(O->getFragmentList().begin());
    Relocation *r3 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                        make<FragmentRef>(*P, 12), -4);
    r3->setTargetRef(make<FragmentRef>(*PLT0, 0));
    O->addRelocation(r3);
  }

  return P;
}
