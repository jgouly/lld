//===- lib/Driver/GnuLdInputGraph.cpp -------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lld/Driver/GnuLdInputGraph.h"
#include "lld/ReaderWriter/LinkerScript.h"

using namespace lld;

/// \brief Parse the input file to lld::File.
error_code ELFFileNode::parse(const LinkingContext &ctx,
                              raw_ostream &diagnostics) {
  ErrorOr<StringRef> filePath = getPath(ctx);
  if (error_code ec = filePath.getError())
    return ec;

  if (error_code ec = getBuffer(*filePath))
    return ec;

  if (ctx.logInputFiles())
    diagnostics << *filePath << "\n";

  if (_attributes._isWholeArchive) {
    std::vector<std::unique_ptr<File>> parsedFiles;
    error_code ec = ctx.registry().parseFile(_buffer, parsedFiles);
    if (ec)
      return ec;
    assert(parsedFiles.size() == 1);
    std::unique_ptr<File> f(parsedFiles[0].release());
    if (auto archive = reinterpret_cast<const ArchiveLibraryFile *>(f.get())) {
      // Have this node own the FileArchive object.
      _archiveFile.reset(archive);
      f.release();
      // Add all members to _files vector
      return archive->parseAllMembers(_files);
    }
    // if --whole-archive is around non-archive, just use it as normal.
    _files.push_back(std::move(f));
    return error_code::success();
  }
  return ctx.registry().parseFile(_buffer, _files);
}

/// \brief Parse the GnuLD Script
error_code GNULdScript::parse(const LinkingContext &ctx,
                              raw_ostream &diagnostics) {
  ErrorOr<StringRef> filePath = getPath(ctx);
  if (error_code ec = filePath.getError())
    return ec;

  if (error_code ec = getBuffer(*filePath))
    return ec;

  if (ctx.logInputFiles())
    diagnostics << *filePath << "\n";

  _lexer.reset(new script::Lexer(std::move(_buffer)));
  _parser.reset(new script::Parser(*_lexer.get()));

  _linkerScript = _parser->parse();

  if (!_linkerScript)
    return LinkerScriptReaderError::parse_error;

  return error_code::success();
}

/// \brief Handle GnuLD script for ELF.
error_code ELFGNULdScript::parse(const LinkingContext &ctx,
                                 raw_ostream &diagnostics) {
  ELFFileNode::Attributes attributes;
  if (error_code ec = GNULdScript::parse(ctx, diagnostics))
    return ec;
  for (const script::Command *c : _linkerScript->_commands) {
    auto *group = dyn_cast<script::Group>(c);
    if (!group)
      continue;
    std::unique_ptr<Group> groupStart(new Group());
    for (const script::Path &path : group->getPaths()) {
      // TODO : Propagate Set WholeArchive/dashlPrefix
      attributes.setAsNeeded(path._asNeeded);
      auto inputNode = new ELFFileNode(
          _elfLinkingContext, _elfLinkingContext.allocateString(path._path),
          attributes);
      std::unique_ptr<InputElement> inputFile(inputNode);
      cast<Group>(groupStart.get())->addFile(std::move(inputFile));
    }
    _expandElements.push_back(std::move(groupStart));
  }
  return error_code::success();
}
