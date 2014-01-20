//===- lib/ReaderWriter/MachO/MachONormalizedFileToAtoms.cpp --------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

///
/// \file Converts from in-memory normalized mach-o to in-memory Atoms.
///
///                  +------------+
///                  | normalized |
///                  +------------+
///                        |
///                        |
///                        v
///                    +-------+
///                    | Atoms |
///                    +-------+

#include "MachONormalizedFile.h"
#include "File.h"
#include "Atoms.h"

#include "lld/Core/LLVM.h"

#include "llvm/Support/MachO.h"

using namespace llvm::MachO;

namespace lld {
namespace mach_o {
namespace normalized {

static uint64_t nextSymbolAddress(const NormalizedFile &normalizedFile,
                                  const Symbol &symbol) {
  uint64_t symbolAddr = symbol.value;
  uint8_t symbolSectionIndex = symbol.sect;
  const Section &section = normalizedFile.sections[symbolSectionIndex - 1];
  // If no symbol after this address, use end of section address.
  uint64_t closestAddr = section.address + section.content.size();
  for (const Symbol &s : normalizedFile.globalSymbols) {
    if (s.sect != symbolSectionIndex)
      continue;
    uint64_t sValue = s.value;
    if (sValue <= symbolAddr)
      continue;
    if (sValue < closestAddr)
      closestAddr = s.value;
  }
  for (const Symbol &s : normalizedFile.localSymbols) {
    if (s.sect != symbolSectionIndex)
      continue;
    uint64_t sValue = s.value;
    if (sValue <= symbolAddr)
      continue;
    if (sValue < closestAddr)
      closestAddr = s.value;
  }
  return closestAddr;
}

static Atom::Scope atomScope(uint8_t scope) {
  switch (scope) {
  case N_EXT:
    return Atom::scopeGlobal;
  case N_PEXT | N_EXT:
    return Atom::scopeLinkageUnit;
  case 0:
    return Atom::scopeTranslationUnit;
  }
  llvm_unreachable("unknown scope value!");
}

bool isCStringSection(const Section &sect) {
  return sect.segmentName == "__TEXT" && sect.sectionName == "__cstring";
}

bool isDataSection(const Section &sect) {
  return sect.segmentName == "__DATA" && sect.sectionName == "__DATA";
}

Symbol find_symbol(const NormalizedFile &NF, unsigned Idx) {
  if (Idx < NF.localSymbols.size())
    return NF.localSymbols[Idx];
  if (Idx < NF.localSymbols.size() + NF.globalSymbols.size())
    return NF.globalSymbols[Idx - NF.localSymbols.size()];
  assert(Idx - NF.localSymbols.size() - NF.globalSymbols.size() <
             NF.undefinedSymbols.size() &&
         "Idx too large!");
  return NF
      .undefinedSymbols[Idx - NF.localSymbols.size() - NF.globalSymbols.size()];
}

const Atom *findAtom(const lld::File &F, const StringRef &Name) {
  for (const auto A : F.undefined()) {
    if (A->name() == Name)
      return A;
  }
  return nullptr;
}

static std::string reloc2str(RelocationInfoType type) {
  switch (type) {
  case llvm::MachO::X86_64_RELOC_UNSIGNED:
    return "X86_64_RELOC_UNSIGNED";
  case llvm::MachO::X86_64_RELOC_SIGNED:
    return "X86_64_RELOC_SIGNED";
  case llvm::MachO::X86_64_RELOC_GOT_LOAD:
    return "X86_64_RELOC_GOT_LOAD";
  case llvm::MachO::X86_64_RELOC_GOT:
    return "X86_64_RELOC_GOT";
  default:
    return "unknown";
  }
}

static Reference::KindValue relocToRefKind(RelocationInfoType type) {
  switch (type) {
  case X86_64_RELOC_BRANCH:
  case X86_64_RELOC_SIGNED:
  case X86_64_RELOC_UNSIGNED:
  case X86_64_RELOC_GOT_LOAD:
  case X86_64_RELOC_GOT:
    return type;
  default:
    llvm_unreachable("unhandled reloc type!");
  }
}

static void processSymbol(const NormalizedFile &normalizedFile, MachOFile &file,
                          const Symbol &sym, bool copyRefs) {
  // Mach-O symbol table does have size in it, so need to scan ahead
  // to find symbol with next highest address.
  const Section &section = normalizedFile.sections[sym.sect - 1];
  uint64_t offset = sym.value - section.address;
  uint64_t size = nextSymbolAddress(normalizedFile, sym) - sym.value;
  ArrayRef<uint8_t> atomContent =
      llvm::makeArrayRef(&section.content[offset], size);
  DefinedAtom::ContentType contentType = DefinedAtom::typeCode;
  if (isCStringSection(section)) {
    contentType = DefinedAtom::typeCString;
  } else if (isDataSection(section))
    contentType = DefinedAtom::typeData;

  MachODefinedAtom *da = file.addDefinedAtom(sym.name, atomContent, contentType,
                                             atomScope(sym.scope), copyRefs);

  for (auto &reloc : normalizedFile.sections[sym.sect - 1].relocations) {
    const Atom *a =
        findAtom(file, find_symbol(normalizedFile, reloc.symbol).name);
    if (!a) {
      a = new SimpleUndefinedAtom(
          file, find_symbol(normalizedFile, reloc.symbol).name);
    }
    file.addAtom(*a);
    da->addReference(Reference::KindNamespace::mach_o,
                     Reference::KindArch::x86_64, relocToRefKind(reloc.type),
                     reloc.offset, a, 0);
  }
}
static ErrorOr<std::unique_ptr<lld::File>>
normalizedObjectToAtoms(const NormalizedFile &normalizedFile, StringRef path,
                        bool copyRefs) {
  std::unique_ptr<MachOFile> file(new MachOFile(path));

  for (const Symbol &sym : normalizedFile.globalSymbols) {
    processSymbol(normalizedFile, *file, sym, copyRefs);
  }

  for (const Symbol &sym : normalizedFile.localSymbols) {
    processSymbol(normalizedFile, *file, sym, copyRefs);
  }

  for (auto &sym : normalizedFile.undefinedSymbols) {
    file->addUndefinedAtom(sym.name, copyRefs);
  }

  return std::unique_ptr<File>(std::move(file));
}

static ErrorOr<std::unique_ptr<lld::File>>
normalizedDylibToAtoms(const NormalizedFile &normalizedFile, StringRef path) {
  std::unique_ptr<DylibFile> file(new DylibFile(path));
  for (auto &sym : normalizedFile.globalSymbols) {
    file->addExportedAtom(sym.name);
  }
  for (auto &sym : normalizedFile.undefinedSymbols) {
    file->addExportedAtom(sym.name);
  }
  return std::unique_ptr<File>(std::move(file));
}

ErrorOr<std::unique_ptr<lld::File>>
normalizedToAtoms(const NormalizedFile &normalizedFile, StringRef path,
                  bool copyRefs) {
  switch (normalizedFile.fileType) {
  case MH_OBJECT:
    return normalizedObjectToAtoms(normalizedFile, path, copyRefs);
  case MH_DYLIB:
    return normalizedDylibToAtoms(normalizedFile, path);
  default:
    llvm_unreachable("unhandled MachO file type!");
  }
}

} // namespace normalized
} // namespace mach_o
} // namespace lld
