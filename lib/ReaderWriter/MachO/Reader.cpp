#include "Reader.h"
#include "lld/ReaderWriter/Reader.h"
#include "lld/Core/SharedLibraryFile.h"

#include "MachONormalizedFile.h"

#include "lld/ReaderWriter/MachOLinkingContext.h"

namespace lld {
namespace mach_o {
bool MachOReader::canParse(file_magic magic, StringRef fileExtension,
                           const MemoryBuffer &mb) const {
  return fileExtension == ".o" || fileExtension == ".dylib";
}

error_code
MachOReader::parseFile(std::unique_ptr<MemoryBuffer> &mb,
                       const class Registry &,
                       std::vector<std::unique_ptr<File>> &result) const {
  ErrorOr<std::unique_ptr<normalized::NormalizedFile>> nf =
      (normalized::readBinary(mb, _arch));
  if (error_code ec = nf.getError())
    return ec;

  result.push_back(
      std::move(*normalized::normalizedToAtoms(
                     *nf->release(), mb->getBufferIdentifier(), false)));
  return error_code::success();
}
} // end namespace mach_o
} // end namespace lld
