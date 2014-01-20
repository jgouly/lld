//===- lld/unittest/MachOTests/MachONormalizedFileToAtomsTests.cpp --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include <llvm/Support/MachO.h>
#include "../../lib/ReaderWriter/MachO/MachONormalizedFile.h"
#include "../../lib/ReaderWriter/MachO/MachONormalizedFileBinaryUtils.h"

#include <assert.h>
#include <vector>

using llvm::ErrorOr;

using namespace lld::mach_o::normalized;
using namespace llvm::MachO;

unsigned countDefinedAtoms(const lld::File &file) {
  unsigned count = 0;
  for (const auto &d : file.defined()) {
    (void)d;
    ++count;
  }
  return count;
}

TEST(ToAtomsTest, empty_obj_x86_64) {
  NormalizedFile f;
  f.arch = lld::MachOLinkingContext::arch_x86_64;
  ErrorOr<std::unique_ptr<const lld::File>> atom_f =
      normalizedToAtoms(f, "", false);
  EXPECT_FALSE(!atom_f);
  EXPECT_EQ(0U, (*atom_f)->defined().size());
}

TEST(ToAtomsTest, basic_obj_x86_64) {
  NormalizedFile f;
  f.arch = lld::MachOLinkingContext::arch_x86_64;
  Section textSection;
  static const uint8_t contentBytes[] = { 0x90, 0xC3, 0xC3, 0xC4 };
  const unsigned contentSize = sizeof(contentBytes) / sizeof(contentBytes[0]);
  textSection.content = llvm::makeArrayRef(contentBytes, contentSize);
  f.sections.push_back(textSection);
  Symbol fooSymbol;
  fooSymbol.name = "_foo";
  fooSymbol.type = N_SECT;
  fooSymbol.scope = N_EXT;
  fooSymbol.sect = 1;
  fooSymbol.value = 0;
  f.globalSymbols.push_back(fooSymbol);
  Symbol barSymbol;
  barSymbol.name = "_bar";
  barSymbol.type = N_SECT;
  barSymbol.scope = N_EXT;
  barSymbol.sect = 1;
  barSymbol.value = 2;
  f.globalSymbols.push_back(barSymbol);
  Symbol undefSym;
  undefSym.name = "_undef";
  undefSym.type = N_UNDF;
  f.undefinedSymbols.push_back(undefSym);
  Symbol bazSymbol;
  bazSymbol.name = "_baz";
  bazSymbol.type = N_SECT;
  bazSymbol.scope = N_EXT | N_PEXT;
  bazSymbol.sect = 1;
  bazSymbol.value = 3;
  f.localSymbols.push_back(bazSymbol);

  ErrorOr<std::unique_ptr<const lld::File>> atom_f =
      normalizedToAtoms(f, "", false);
  EXPECT_FALSE(!atom_f);
  const lld::File &file = **atom_f;
  EXPECT_EQ(3U, file.defined().size());
  lld::File::defined_iterator it = file.defined().begin();
  const lld::DefinedAtom *atom1 = *it;
  ++it;
  const lld::DefinedAtom *atom2 = *it;
  ++it;
  const lld::DefinedAtom *atom3 = *it;
  const lld::UndefinedAtom *atom4 = *file.undefined().begin();
  EXPECT_TRUE(atom1->name().equals("_foo"));
  EXPECT_EQ(2U, atom1->rawContent().size());
  EXPECT_EQ(0x90, atom1->rawContent()[0]);
  EXPECT_EQ(0xC3, atom1->rawContent()[1]);
  EXPECT_EQ(lld::Atom::scopeGlobal, atom1->scope());

  EXPECT_TRUE(atom2->name().equals("_bar"));
  EXPECT_EQ(1U, atom2->rawContent().size());
  EXPECT_EQ(0xC3, atom2->rawContent()[0]);
  EXPECT_EQ(lld::Atom::scopeGlobal, atom2->scope());

  EXPECT_TRUE(atom3->name().equals("_baz"));
  EXPECT_EQ(1U, atom3->rawContent().size());
  EXPECT_EQ(0xC4, atom3->rawContent()[0]);
  EXPECT_EQ(lld::Atom::scopeLinkageUnit, atom3->scope());

  EXPECT_TRUE(atom4->name().equals("_undef"));
  EXPECT_EQ(lld::Atom::definitionUndefined, atom4->definition());
}

TEST(ToAtomsTest, hello_world_obj_x86_64) {
  NormalizedFile f;
  f.arch = lld::MachOLinkingContext::arch_x86_64;
  Section textSection;
  static const uint8_t contentBytes[] = { 0x55, 0x48, 0x89, 0xE5,
                                          0x31, 0xC0, 0x5D, 0xC3 };
  const unsigned contentSize = sizeof(contentBytes) / sizeof(contentBytes[0]);
  textSection.content = llvm::makeArrayRef(contentBytes, contentSize);
  f.sections.push_back(textSection);
  Symbol mainSymbol;
  mainSymbol.name = "_main";
  mainSymbol.type = N_SECT;
  mainSymbol.sect = 1;
  f.globalSymbols.push_back(mainSymbol);
  ErrorOr<std::unique_ptr<const lld::File>> atom_f =
      normalizedToAtoms(f, "", false);
  EXPECT_FALSE(!atom_f);
  EXPECT_EQ(1U, countDefinedAtoms(**atom_f));
  const lld::DefinedAtom *singleAtom = *(*atom_f)->defined().begin();
  llvm::ArrayRef<uint8_t> atomContent(singleAtom->rawContent());
  EXPECT_EQ(0, memcmp(atomContent.data(), contentBytes, contentSize));
}

TEST(ToAtomsTest, global_str_obj_x86_64) {
  NormalizedFile f;
  f.arch = lld::MachOLinkingContext::arch_x86_64;
  Section CStringSection;
  static const uint8_t contentBytes[] = { 0x68, 0x65, 0x6c, 0x6c, 0x6f,
                                          0x20, 0x77, 0x6f, 0x72, 0x6c,
                                          0x64, 0x21, 0x0a, 0x00 };
  const unsigned contentSize = sizeof(contentBytes) / sizeof(contentBytes[0]);
  CStringSection.content = llvm::makeArrayRef(contentBytes, contentSize);
  f.sections.push_back(CStringSection);
  Section DataSection;
  static const uint8_t dataContentBytes[] = { 0x00, 0x00, 0x00, 0x00,
                                              0x00, 0x00, 0x00, 0x00 };
  const unsigned dataContentSize =
      sizeof(dataContentBytes) / sizeof(dataContentBytes[0]);
  DataSection.content = llvm::makeArrayRef(dataContentBytes, dataContentSize);
  llvm::MachO::any_relocation_info reloc = { 0x0, 0xe000000 };
  DataSection.relocations.push_back(unpackRelocation(reloc, false, false));
  f.sections.push_back(DataSection);

  Symbol localStrSymbol;
  localStrSymbol.name = "L_.str";
  localStrSymbol.type = N_SECT;
  localStrSymbol.sect = 1;
  f.localSymbols.push_back(localStrSymbol);
  Symbol strSymbol;
  strSymbol.name = "_str";
  strSymbol.type = N_SECT;
  strSymbol.sect = 2;
  f.globalSymbols.push_back(strSymbol);
  ErrorOr<std::unique_ptr<const lld::File>> atom_f =
      normalizedToAtoms(f, "", false);
  EXPECT_FALSE(!atom_f);
  EXPECT_EQ(2U, countDefinedAtoms(**atom_f));
  auto it = (*atom_f)->defined().begin();
  const lld::DefinedAtom *singleAtom = *(*atom_f)->defined().begin();
  llvm::ArrayRef<uint8_t> atomContent(singleAtom->rawContent());
  EXPECT_EQ(0, memcmp(atomContent.data(), dataContentBytes, dataContentSize));
}
