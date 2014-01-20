//===- lib/ReaderWriter/MachO/File.h --------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_READER_WRITER_MACHO_FILE_H
#define LLD_READER_WRITER_MACHO_FILE_H

#include "Atoms.h"

#include "lld/Core/SharedLibraryFile.h"
#include "lld/ReaderWriter/Simple.h"

namespace lld {
namespace mach_o {

class MachOFile : public SimpleFile {
public:
  MachOFile(StringRef path) : SimpleFile(path) {}

  MachODefinedAtom *addDefinedAtom(StringRef name, ArrayRef<uint8_t> content,
                                   DefinedAtom::ContentType contentType,
                                   Atom::Scope scope, bool copyRefs) {
    if (copyRefs) {
      // Make a copy of the atom's name and content that is owned by this file.
      name = name.copy(_allocator);
      content = content.copy(_allocator);
    }
    MachODefinedAtom *atom = new (_allocator)
        MachODefinedAtom(*this, name, content, contentType, scope);
    addAtom(*atom);
    return atom;
  }

  void addUndefinedAtom(StringRef name, bool copyRefs) {
    if (copyRefs) {
      // Make a copy of the atom's name that is owned by this file.
      name = name.copy(_allocator);
    }
    SimpleUndefinedAtom *atom =
        new (_allocator) SimpleUndefinedAtom(*this, name);
    addAtom(*atom);
  }

private:
  llvm::BumpPtrAllocator _allocator;
};

class DylibFile : public SharedLibraryFile {
public:
  DylibFile(StringRef path) : SharedLibraryFile(path) {}

  virtual const SharedLibraryAtom *exports(StringRef name,
                                           bool dataSymbolOnly) const {
    if (exportedAtoms.find(name) == exportedAtoms.end()) {
      return nullptr;
    }
    if (exportedAtoms[name] == nullptr) {
      exportedAtoms[name] = new (_allocator) DylibAtom(*this, name, path());
    }
    return exportedAtoms[name];
  }
  void addUndefinedAtom(const SimpleUndefinedAtom *undef) {
    _undefinedAtoms._atoms.push_back(undef);
  }
  void addCodeAtom(MachODefinedAtom *CA) { _definedAtoms._atoms.push_back(CA); }
  virtual const atom_collection<DefinedAtom> &defined() const {
    return _definedAtoms;
  }
  void addExportedAtom(StringRef name) { exportedAtoms[name] = nullptr; }
  virtual const atom_collection<UndefinedAtom> &undefined() const {
    return _undefinedAtoms;
  }

  virtual const atom_collection<SharedLibraryAtom> &sharedLibrary() const {
    return _sharedLibraryAtoms;
  }

  virtual const atom_collection<AbsoluteAtom> &absolute() const {
    return _absoluteAtoms;
  }

private:
  atom_collection_vector<DefinedAtom> _definedAtoms;
  atom_collection_vector<UndefinedAtom> _undefinedAtoms;
  atom_collection_vector<SharedLibraryAtom> _sharedLibraryAtoms;
  atom_collection_vector<AbsoluteAtom> _absoluteAtoms;
  mutable std::map<StringRef, SharedLibraryAtom *> exportedAtoms;
  mutable llvm::BumpPtrAllocator _allocator;
};

} // end namespace mach_o
} // end namespace lld

#endif
