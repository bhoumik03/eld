#ifndef TARGET_X86_64_X86_64ELFDYNAMIC_H
#define TARGET_X86_64_X86_64ELFDYNAMIC_H

#include "eld/Target/ELFDynamic.h"

namespace eld {

class x86_64ELFDynamic : public ELFDynamic {
public:
  x86_64ELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig);
  ~x86_64ELFDynamic();

private:
  void reserveTargetEntries() override;
  void applyTargetEntries() override;
};
} // namespace eld
#endif
