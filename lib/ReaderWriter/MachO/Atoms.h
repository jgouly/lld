//===- lib/ReaderWriter/MachO/Atoms.h -------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_READER_WRITER_MACHO_ATOMS_H
#define LLD_READER_WRITER_MACHO_ATOMS_H

#include "lld/ReaderWriter/Simple.h"

namespace lld {
namespace mach_o {
class MachODefinedAtom : public SimpleDefinedAtom {
public:
  // FIXME: This constructor should also take the ContentType.
  MachODefinedAtom(const File &f, const StringRef name,
                   const ArrayRef<uint8_t> content, ContentType contentType,
                   Scope scope)
      : SimpleDefinedAtom(f), _name(name), _content(content),
        _contentType(contentType), _scope(scope) {}

  uint64_t size() const override { return rawContent().size(); }

  ContentType contentType() const override { return _contentType; }

  StringRef name() const override { return _name; }

  Scope scope() const override { return _scope; }

  ArrayRef<uint8_t> rawContent() const override { return _content; }

private:
  const StringRef _name;
  const ArrayRef<uint8_t> _content;
  const ContentType _contentType;
  const Scope _scope;
};

class DylibAtom : public SharedLibraryAtom {
public:
  DylibAtom(const File &file, StringRef name, StringRef path)
      : SharedLibraryAtom(), _file(file), _name(name), _path(path) {}

  virtual const File &file() const { return _file; }

  virtual Type type() const { return Type::Code; }

  virtual StringRef loadName() const { return file().path(); }

  virtual uint64_t size() const { return 0; }

  virtual bool canBeNullAtRuntime() const { return false; }

  virtual StringRef name() const { return _name; }

private:
  const File &_file;
  StringRef _name;
  StringRef _path;
};
} // mach_o
} // lld

#endif
