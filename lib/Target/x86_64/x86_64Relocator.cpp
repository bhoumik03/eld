//===- x86_64Relocator.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "eld/Config/GeneralOptions.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "x86_64PLT.h"
#include "x86_64RelocationFunctions.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

//===--------------------------------------------------------------------===//
// x86_64Relocator
//===--------------------------------------------------------------------===//
x86_64Relocator::x86_64Relocator(x86_64LDBackend &pParent,
                                 LinkerConfig &pConfig, Module &pModule)
    : Relocator(pConfig, pModule), m_Target(pParent) {
  // Mark force verify bit for specified relcoations
  if (m_Module.getPrinter()->verifyReloc() &&
      config().options().verifyRelocList().size()) {
    auto &list = config().options().verifyRelocList();
    for (auto &i : x86RelocDesc) {
      auto RelocInfo = x86_64Relocs[i.type];
      if (list.find(RelocInfo.Name) != list.end())
        i.forceVerify = true;
    }
  }
}

x86_64Relocator::~x86_64Relocator() {}

Relocator::Result x86_64Relocator::applyRelocation(Relocation &pRelocation) {
  Relocation::Type type = pRelocation.type();

  ResolveInfo *symInfo = pRelocation.symInfo();

  if (type > x86_64_MAXRELOCS)
    return Relocator::Unknown;

  if (symInfo) {
    LDSymbol *outSymbol = symInfo->outSymbol();
    if (outSymbol && outSymbol->hasFragRef()) {
      ELFSection *S = outSymbol->fragRef()->frag()->getOwningSection();
      if (S->isDiscard() ||
          (S->getOutputSection() && S->getOutputSection()->isDiscard())) {
        std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
        issueUndefRef(pRelocation, *S->getInputFile(), S);
        return Relocator::OK;
      }
    }
  }

  // apply the relocation
  return x86RelocDesc[type].func(pRelocation, *this, x86RelocDesc[type]);
}

const char *x86_64Relocator::getName(Relocation::Type pType) const {

  return x86_64Relocs[pType].Name;
}

Relocator::Size x86_64Relocator::getSize(Relocation::Type pType) const {
  return x86_64Relocs[pType].Size;
}

// Check if the relocation is invalid
bool x86_64Relocator::isInvalidReloc(Relocation &pReloc) const {

  switch (pReloc.type()) {
  case llvm::ELF::R_X86_64_NONE:
  case llvm::ELF::R_X86_64_64:
  case llvm::ELF::R_X86_64_PC32:
  case llvm::ELF::R_X86_64_COPY:
  case llvm::ELF::R_X86_64_32:
  case llvm::ELF::R_X86_64_32S:
  case llvm::ELF::R_X86_64_16:
  case llvm::ELF::R_X86_64_PC16:
  case llvm::ELF::R_X86_64_8:
  case llvm::ELF::R_X86_64_PC8:
  case llvm::ELF::R_X86_64_PC64:
  case llvm::ELF::R_X86_64_PLT32:
  case llvm::ELF::R_X86_64_GOTPCREL:
  case llvm::ELF::R_X86_64_GOTPCRELX:
  case llvm::ELF::R_X86_64_REX_GOTPCRELX:
  case llvm::ELF::R_X86_64_TPOFF32:
  case llvm::ELF::R_X86_64_TPOFF64:
  case llvm::ELF::R_X86_64_DTPOFF32:
  case llvm::ELF::R_X86_64_DTPOFF64:
    return false;
  default:
    return true; // Other Relocations are not supported as of now
  }
}

void x86_64Relocator::scanRelocation(Relocation &pReloc,
                                     eld::IRBuilder &pLinker,
                                     ELFSection &pSection,
                                     InputFile &pInputFile,
                                     CopyRelocs &CopyRelocs) {
  if (LinkerConfig::Object == config().codeGenType())
    return;

  // If we are generating a shared library check for invalid relocations
  if (isInvalidReloc(pReloc)) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    ::llvm::outs() << getName(pReloc.type()) << " not supported currently\n";
    m_Target.getModule().setFailure(true);
    return;
  }

  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  assert(nullptr != rsym &&
         "ResolveInfo of relocation not set while scanRelocation");

  // Check if we are tracing relocations.
  if (m_Module.getPrinter()->traceReloc()) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    std::string relocName = getName(pReloc.type());
    if (config().options().traceReloc(relocName))
      config().raise(Diag::reloc_trace)
          << relocName << pReloc.symInfo()->name()
          << pInputFile.getInput()->decoratedPath();
  }

  // check if we should issue undefined reference for the relocation target
  // symbol
  if (rsym->isUndef() || rsym->isBitCode()) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (!m_Target.canProvideSymbol(rsym)) {
      if (m_Target.canIssueUndef(rsym)) {
        if (rsym->visibility() != ResolveInfo::Default)
          issueInvisibleRef(pReloc, pInputFile);
        issueUndefRef(pReloc, pInputFile, &pSection);
      }
    }
  }
  ELFSection *section = pSection.getLink()
                            ? pSection.getLink()
                            : pReloc.targetRef()->frag()->getOwningSection();

  if (!section->isAlloc())
    return;

  if (rsym->isLocal()) // rsym is local
    scanLocalReloc(pInputFile, pReloc, pLinker, *section);
  else // rsym is external
    scanGlobalReloc(pInputFile, pReloc, pLinker, *section, CopyRelocs);
}

namespace {

Relocation *helper_DynRel_init(ELFObjectFile *Obj, Relocation *R,
                               ResolveInfo *pSym, Fragment *F, uint32_t pOffset,
                               Relocator::Type pType, x86_64LDBackend &B) {
  Relocation *rela_entry = Obj->getRelaDyn()->createOneReloc();

  rela_entry->setType(pType);
  rela_entry->setTargetRef(make<FragmentRef>(*F, pOffset));
  rela_entry->setSymInfo(pSym);

  if (pType == llvm::ELF::R_X86_64_GLOB_DAT) {
    rela_entry->setAddend(0);
  } else if (pType == llvm::ELF::R_X86_64_RELATIVE) {
    // For RELATIVE relocations, the dynamic writer will emit addend as
    // getSymValue(R) + R->addend().
    // R->addend() is the original relocation addend (A).
    // For RELATIVE created from GOTPCREL forms, ignore the instruction-local
    // bias (-4) by setting addend to 0.
    int64_t A = 0;
    if (R) {
      auto Rt = R->type();
      if (Rt == llvm::ELF::R_X86_64_GOTPCREL ||
          Rt == llvm::ELF::R_X86_64_GOTPCRELX ||
          Rt == llvm::ELF::R_X86_64_REX_GOTPCRELX) {
        A = 0;
      } else {
        A = R->addend();
      }
    }
    rela_entry->setAddend(A);
  } else if (R) {
    rela_entry->setAddend(R->addend());
  } else {
    rela_entry->setAddend(0);
  }

  if (R && (pType == llvm::ELF::R_X86_64_RELATIVE)) {
    B.recordRelativeReloc(rela_entry, R);
  }
  return rela_entry;
}

x86_64GOT &CreateGOT(ELFObjectFile *Obj, Relocation &pReloc, bool pHasRel,
                     x86_64LDBackend &B, bool isExec) {
  ResolveInfo *rsym = pReloc.symInfo();
  x86_64GOT *G = B.createGOT(GOT::Regular, Obj, rsym);

  if (!pHasRel) {
    G->setValueType(GOT::SymbolValue);
    return *G;
  }

  // In PIC (PIE/shared), GOT entries for non-preemptible symbols should use
  // RELATIVE. Preemptible symbols use GLOB_DAT. Do not gate RELATIVE on
  // executable-vs-shared; PIE is still PIC.
  bool useRelative = !B.isSymbolPreemptible(*rsym);

  helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                     useRelative ? llvm::ELF::R_X86_64_RELATIVE
                                 : llvm::ELF::R_X86_64_GLOB_DAT,
                     B);

  if (useRelative) {
    G->setValueType(GOT::SymbolValue);
  }

  return *G;
}

} // namespace

void x86_64Relocator::scanLocalReloc(InputFile &pInputFile, Relocation &pReloc,
                                     eld::IRBuilder &pBuilder,
                                     ELFSection &pSection) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInputFile);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();

  // Special case when the linker makes a symbol local for example linker
  // defined symbols such as _DYNAMIC
  switch (pReloc.type()) {
  case llvm::ELF::R_X86_64_GOTPCREL:
  case llvm::ELF::R_X86_64_GOTPCRELX:
  case llvm::ELF::R_X86_64_REX_GOTPCRELX: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->reserved() & ReserveGOT)
      return;
    // Even for local symbols and static linking, create a GOT entry and fill
    // it at link time . This avoids implementing relaxations now.
    x86_64GOT &G = CreateGOT(Obj, pReloc, config().isCodeIndep(), m_Target,
                             config().codeGenType() == LinkerConfig::Exec);
    (void)G;
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }
  default:
    break;
  }
}

void x86_64Relocator::scanGlobalReloc(InputFile &pInputFile, Relocation &pReloc,
                                      eld::IRBuilder &pBuilder,
                                      ELFSection &pSection, CopyRelocs &) {

  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInputFile);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();

  switch (pReloc.type()) {
  case llvm::ELF::R_X86_64_PLT32: {
    // Static code: treat PLT32 as a normal PC-relative call/jmp.
    if (config().isCodeStatic())
      return;
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // Determine if the target is external to this link unit.
    const bool isLocal = rsym->isLocal();
    const bool isDefinedHere = rsym->isDefine() && !rsym->isDyn();
    const bool isExternal = !(isLocal || isDefinedHere);
    // Local or link-time known definitions: keep PC-relative, no PLT.
    if (!isExternal)
      return;
    // Only functions use PLT entries; PLT32 for data shouldn't synthesize PLT.
    if (rsym->type() != ResolveInfo::Function)
      return;
    // External function: create PLT if not already present.
    if (!(rsym->reserved() & ReservePLT)) {
      m_Target.createPLT(Obj, rsym);
      rsym->setReserved(rsym->reserved() | ReservePLT);
    }
    return;
  }
  case llvm::ELF::R_X86_64_GOTPCREL:
  case llvm::ELF::R_X86_64_GOTPCRELX:
  case llvm::ELF::R_X86_64_REX_GOTPCRELX: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);

    if (rsym->reserved() & ReserveGOT)
      return;

    CreateGOT(Obj, pReloc, !config().isCodeStatic(), m_Target,
              config().codeGenType() == LinkerConfig::Exec);

    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  } break;
  default:
    break;

  } // end of switch
}

void x86_64Relocator::defineSymbolforGuard(eld::IRBuilder &pBuilder,
                                           ResolveInfo *pSym,
                                           x86_64LDBackend &pTarget) {
  return;
}

void x86_64Relocator::partialScanRelocation(Relocation &pReloc,
                                            const ELFSection &pSection) {
  pReloc.updateAddend(module());

  // if we meet a section symbol
  if (pReloc.symInfo()->type() == ResolveInfo::Section) {
    LDSymbol *input_sym = pReloc.symInfo()->outSymbol();

    // 1. update the relocation target offset
    assert(input_sym->hasFragRef());
    // 2. get the output ELFSection which the symbol defined in
    ELFSection *out_sect = input_sym->fragRef()->getOutputELFSection();

    ResolveInfo *sym_info = m_Module.getSectionSymbol(out_sect);
    // set relocation target symbol to the output section symbol's resolveInfo
    pReloc.setSymInfo(sym_info);
  }
}

uint32_t x86_64Relocator::getNumRelocs() const { return x86_64_MAXRELOCS; }

//=========================================//
// Relocation Verifier
//=========================================//
template <typename T>
Relocator::Result VerifyRelocAsNeededHelper(
    Relocation &pReloc, T Result, const RelocationDescription &pRelocDesc,
    DiagnosticEngine *DiagEngine, const GeneralOptions &options) {
  uint32_t RelocType = pReloc.type();
  auto RelocInfo = x86_64Relocs[RelocType];
  Relocator::Result R = Relocator::OK;

  Result >>= x86_64Relocs[RelocType].Shift;

  if (RelocInfo.VerifyRange) {
    if (!verifyRangeX86_64(RelocInfo, Result))
      R = Relocator::Overflow;
  }

  if ((pRelocDesc.forceVerify) && (isTruncatedX86_64(RelocInfo, Result))) {
    DiagEngine->raise(Diag::reloc_truncated)
        << RelocInfo.Name << pReloc.symInfo()->name()
        << pReloc.getTargetPath(options) << pReloc.getSourcePath(options);
  }
  return R;
}

void x86_64Relocator::computeTLSOffsets() {
  std::vector<ELFSegment *> tlsSegments =
      getTarget().elfSegmentTable().getSegments(llvm::ELF::PT_TLS);

  if (tlsSegments.empty()) {
    return;
  }

  ASSERT(tlsSegments.size() == 1,
         "Multiple TLS segments not supported in x86_64 backend");

  ELFSegment *tlsSegment = tlsSegments[0];
  uint64_t templateSize = tlsSegment->memsz();
  uint64_t alignment = tlsSegment->align();
  templateSize = llvm::alignTo(templateSize, alignment);
  GNULDBackend::setTLSTemplateSize(templateSize);
}

template <typename T>
Relocator::Result ApplyReloc(Relocation &pReloc, T Result,
                             const RelocationDescription &pRelocDesc,
                             DiagnosticEngine *DiagEngine,
                             const GeneralOptions &options) {
  auto RelocInfo = x86_64Relocs[pReloc.type()];

  // Verify the Relocation.
  Relocator::Result R = Relocator::OK;
  R = VerifyRelocAsNeededHelper(pReloc, Result, pRelocDesc, DiagEngine,
                                options);
  if (R != Relocator::OK)
    return R;

  // Apply the relocation
  pReloc.target() = doRelocX86_64(RelocInfo, pReloc.target(), Result);
  return R;
}

//=========================================//
// Each relocation function implementation //
//=========================================//
// R_X86_64_NONE
Relocator::Result eld::none(Relocation &pReloc, x86_64Relocator &pParent,
                            RelocationDescription &pRelocDesc) {
  return Relocator::OK;
}

Relocator::Result applyRel(Relocation &pReloc, uint32_t Result,
                           const RelocationDescription &pRelocDesc,
                           DiagnosticEngine *DiagEngine,
                           const GeneralOptions &options) {
  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine, options);
}

Relocator::Result eld::relocAbs(Relocation &pReloc, x86_64Relocator &pParent,
                                RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pReloc.symValue(pParent.module());
  Relocator::DWord A = pReloc.addend();
  const GeneralOptions &options = pParent.config().options();
  // For absolute relocations, and If we are building a static executable and if
  // the symbol is a weak undefined symbol, it should still use the undefined
  // symbol value which is 0. For non absolute relocations, the call is set to a
  // symbol defined by the linker which returns back to the caller.
  if (rsym && rsym->isWeakUndef() &&
      (pParent.config().codeGenType() == LinkerConfig::Exec)) {
    S = 0;
    return ApplyReloc(pReloc, S + A, pRelocDesc, DiagEngine, options);
  }

  // if the flag of target section is not ALLOC, we eprform only static
  // relocation.
  if (!pReloc.targetRef()->getOutputELFSection()->isAlloc()) {
    return ApplyReloc(pReloc, S + A, pRelocDesc, DiagEngine, options);
  }

  // FIXME PLT STUFF
  //  if (rsym && rsym->reserved() & Relocator::ReservePLT)
  //    S =
  //    pParent.getTarget().findEntryInPLT(rsym)->getAddr(config().getDiagEngine());

  return ApplyReloc(pReloc, S + A, pRelocDesc, DiagEngine, options);
}

Relocator::Result eld::relocPCREL(Relocation &pReloc, x86_64Relocator &pParent,
                                  RelocationDescription &pRelocDesc) {
  //  ResolveInfo *rsym = pReloc.symInfo();
  uint32_t Result;
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  Relocator::Address S = pReloc.symValue(pParent.module());
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());

  FragmentRef *target_fragref = pReloc.targetRef();
  Fragment *target_frag = target_fragref->frag();
  ELFSection *target_sect = target_frag->getOutputELFSection();

  Result = S + A - P;
  const GeneralOptions &options = pParent.config().options();
  // for relocs inside non ALLOC, just apply
  if (!target_sect->isAlloc()) {
    return applyRel(pReloc, Result, pRelocDesc, DiagEngine, options);
  }

  // FIXME PLT STUFF
  //  if (!rsym->isLocal()) {
  //    if (rsym->reserved() & Relocator::ReservePLT) {
  //      S =
  //      pParent.getTarget().findEntryInPLT(rsym)->getAddr(config().getDiagEngine());
  //      Result = S + A - P;
  //      applyRel(pReloc, Result, pRelocDesc, DiagEngine);
  //      return Relocator::OK;
  //    }
  //  }

  return applyRel(pReloc, Result, pRelocDesc, DiagEngine, options);
}

// R_X86_64_PLT32 - PC-relative 32-bit relocation for function calls
// Formula: S + A - P (or PLT_entry + A - P if symbol has PLT)
Relocator::Result eld::relocPLT32(Relocation &pReloc, x86_64Relocator &pParent,
                                  RelocationDescription &pRelocDesc) {
  uint32_t Result;
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  Relocator::Address S = pReloc.symValue(pParent.module());

  // For external functions, redirect through PLT
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT) {
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(pParent.config().getDiagEngine());
  }
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());

  Result = S + A - P;
  const GeneralOptions &options = pParent.config().options();

  return applyRel(pReloc, Result, pRelocDesc, DiagEngine, options);
}

Relocator::Result eld::unsupport(Relocation &pReloc, x86_64Relocator &pParent,
                                 RelocationDescription &pRelocDesc) {
  return x86_64Relocator::Unsupport;
}

Relocator::Result eld::relocGOTPCREL(Relocation &pReloc,
                                     x86_64Relocator &pParent,
                                     RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *symInfo = pReloc.symInfo();
  const GeneralOptions &options = pParent.config().options();

  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());
  // Calculate GOTPCREL: GOT[S] + A - P
  x86_64GOT *gotEntry = pParent.getTarget().findEntryInGOT(symInfo);
  uint64_t Result = gotEntry->getAddr(DiagEngine) + A - P;

  return applyRel(pReloc, Result, pRelocDesc, DiagEngine, options);
}

Relocator::Result eld::relocTPOFF(Relocation &pReloc, x86_64Relocator &pParent,
                                  RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  const GeneralOptions &options = pParent.config().options();

  uint64_t TLSTemplateSize = pParent.getTarget().getTLSTemplateSize();

  if (TLSTemplateSize == 0) {
    pParent.config().raise(Diag::no_pt_tls_segment);
    return Relocator::BadReloc;
  }

  uint64_t S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  uint64_t Result = S + A - TLSTemplateSize;

  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine, options);
}

Relocator::Result eld::relocDTPOFF(Relocation &pReloc, x86_64Relocator &pParent,
                                   RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  const GeneralOptions &options = pParent.config().options();
  uint64_t S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  int64_t Result = S + A;
  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine, options);
}
