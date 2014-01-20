#include "lld/ReaderWriter/Reader.h"

#include "MachONormalizedFile.h"

#include "lld/ReaderWriter/MachOLinkingContext.h"

namespace lld {
namespace mach_o {
class MachOReader : public Reader {
public:
  MachOReader(MachOLinkingContext::Arch arch) : Reader(), _arch(arch) {}
  virtual bool canParse(file_magic magic, StringRef fileExtension,
                        const MemoryBuffer &mb) const override;
  virtual error_code
  parseFile(std::unique_ptr<MemoryBuffer> &mb, const class Registry &,
            std::vector<std::unique_ptr<File>> &result) const override;

private:
  MachOLinkingContext::Arch _arch;
};
}
std::unique_ptr<Reader> createReaderMachO();
}
