//===- x86_64GOT.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_TARGET_x86_64_GOT_H
#define ELD_TARGET_x86_64_GOT_H

#include "eld/Core/Module.h"
#include "eld/Fragment/GOT.h"
#include "eld/Support/Memory.h"
#include "eld/Target/GNULDBackend.h"

namespace eld {

/** \class x86_64GOT
 *  \brief x86_64 Global Offset Table.
 */

class x86_64GOT : public GOT {
public:
  // Going to be used by GOTPLT0
  x86_64GOT(GOTType T, ELFSection *O, ResolveInfo *R, uint32_t Align,
            uint32_t Size)
      : GOT(T, O, R, Align, Size), Value(0) {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  // Helper constructor for GOT.
  x86_64GOT(GOTType T, ELFSection *O, ResolveInfo *R)
      : GOT(T, O, R, /*Align=*/8, /*Size=*/8) {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  virtual ~x86_64GOT() {}

  virtual x86_64GOT *getFirst() { return this; }

  virtual x86_64GOT *getNext() { return nullptr; }

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    Value = 0;
    // If the GOT contents needs to reflect a symbol value, then we use the
    // symbol value.
    if (getValueType() == GOT::SymbolValue)
      Value = symInfo()->outSymbol()->value();
    llvm::outs() << "\nGot entry value: 0x" << llvm::format("%x", Value)
                 << "\n";
    if (getValueType() == GOT::TLSStaticSymbolValue)
      Value =
          symInfo()->outSymbol()->value() - GNULDBackend::getTLSTemplateSize();

    return llvm::ArrayRef(reinterpret_cast<const uint8_t *>(&Value),
                          sizeof(Value));
  }

  static x86_64GOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<x86_64GOT>(GOT::Regular, O, R);
  }

private:
  mutable uint64_t Value;
};

// class x86_64GOTPLT0 : public x86_64GOT {
// public:
//   x86_64GOTPLT0(ELFSection *O, ResolveInfo *R)
//       : x86_64GOT(GOT::GOTPLT0, O, R, /*Align=*/8, /*Size=*/24) {}

//   x86_64GOT *getFirst() override { return this; }

//   x86_64GOT *getNext() override { return nullptr; }

//   static x86_64GOTPLT0 *Create(ELFSection *O, ResolveInfo *R);
// };

class x86_64GOTPLT0 : public x86_64GOT {
public:
  x86_64GOTPLT0(ELFSection *O, Module *M)
      : x86_64GOT(GOT::GOTPLT0, O, nullptr, /*Align=*/8, /*Size=*/24),
        m_Module(M) {}

  x86_64GOT *getFirst() override { return this; }
  x86_64GOT *getNext() override { return nullptr; }

  virtual llvm::ArrayRef<uint8_t> getContent() const override {

    if (m_Module) {
      ELFSection *dynSection = m_Module->getSection(".dynamic");
      if (dynSection) {
        uint64_t dynAddr = dynSection->addr();
        memcpy(Value, &dynAddr, sizeof(uint64_t));
      }
    }

    return llvm::ArrayRef(Value, sizeof(Value));
  }

  static x86_64GOTPLT0 *Create(ELFSection *O, Module *M);

private:
  Module *m_Module;
  mutable uint8_t Value[24];
};

// class x86_64GOTPLT0 : public x86_64GOT {
// public:
//   x86_64GOTPLT0(ELFSection *O, ResolveInfo *R)
//       : x86_64GOT(GOT::GOTPLT0, O, R, /*Align=*/8, /*Size=*/24) {}

//   virtual llvm::ArrayRef<uint8_t> getContent() const override {
//     memset(Value, 0, sizeof(Value));

//     if (symInfo() && symInfo()->outSymbol()) {
//       uint64_t dynAddr = symInfo()->outSymbol()->value();
//       memcpy(Value, &dynAddr, sizeof(uint64_t));
//     }

//     return llvm::ArrayRef(Value, sizeof(Value));
//   }

//   static x86_64GOTPLT0 *Create(ELFSection *O, ResolveInfo *R);

// private:
//   mutable uint8_t Value[24];
// };

// class x86_64GOTPLTN : public x86_64GOT {
// public:
//   x86_64GOTPLTN(ELFSection *O, ResolveInfo *R)
//       : x86_64GOT(GOT::GOTPLTN, O, R, /*Align=*/8, /*Size=*/8) {}

//   x86_64GOT *getFirst() override { return this; }

//   x86_64GOT *getNext() override { return nullptr; }

//   static x86_64GOTPLTN *Create(ELFSection *O, ResolveInfo *R) {
//     return make<x86_64GOTPLTN>(O, R);
//   }
// };

class x86_64GOTPLTN : public x86_64GOT {
public:
  x86_64GOTPLTN(ELFSection *O, ResolveInfo *R)
      : x86_64GOT(GOT::GOTPLTN, O, R, /*Align=*/8, /*Size=*/8),
        m_PLTEntry(nullptr) {}

  x86_64GOT *getFirst() override { return this; }
  x86_64GOT *getNext() override { return nullptr; }

  void setPLTEntry(Fragment *plt) { m_PLTEntry = plt; }

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    Value = 0;

    if (m_PLTEntry) {
      ELFSection *pltSection =
          m_PLTEntry->getOwningSection()->getOutputELFSection();
      if (pltSection) {
        llvm::outs() << "PLT section addr: 0x"
                     << llvm::format("%x", pltSection->pAddr()) << "\n";
        llvm::outs() << "m_PLTEntry->getOffset()" << m_PLTEntry->getOffset()
                     << "\n";
        Value = pltSection->addr() + m_PLTEntry->getOffset() + 6;
      }
    }

    return llvm::ArrayRef(reinterpret_cast<const uint8_t *>(&Value),
                          sizeof(Value));
  }

  static x86_64GOTPLTN *Create(ELFSection *O, ResolveInfo *R);

private:
  Fragment *m_PLTEntry;
  mutable uint64_t Value;
};

class x86_64GDGOT : public x86_64GOT {
public:
  x86_64GDGOT(ELFSection *O, ResolveInfo *R)
      : x86_64GOT(GOT::TLS_GD, O, R),
        Other(make<x86_64GOT>(GOT::TLS_GD, O, R)) {}

  x86_64GOT *getFirst() override { return this; }

  x86_64GOT *getNext() override { return Other; }

  static x86_64GOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<x86_64GDGOT>(O, R);
  }

private:
  x86_64GOT *Other;
};

class x86_64LDGOT : public x86_64GOT {
public:
  x86_64LDGOT(ELFSection *O, ResolveInfo *R)
      : x86_64GOT(GOT::TLS_LD, O, R),
        Other(make<x86_64GOT>(GOT::TLS_LD, O, R)) {}

  x86_64GOT *getFirst() override { return this; }

  x86_64GOT *getNext() override { return Other; }

  static x86_64GOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<x86_64LDGOT>(O, R);
  }

private:
  x86_64GOT *Other;
};

class x86_64IEGOT : public x86_64GOT {
public:
  x86_64IEGOT(ELFSection *O, ResolveInfo *R) : x86_64GOT(GOT::TLS_LE, O, R) {}

  x86_64GOT *getFirst() override { return this; }

  x86_64GOT *getNext() override { return nullptr; }

  static x86_64GOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<x86_64IEGOT>(O, R);
  }
};
} // namespace eld

#endif
