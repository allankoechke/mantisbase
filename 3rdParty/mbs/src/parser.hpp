#pragma once

#include "ast.hpp"
#include "lexer.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace mbs {

struct ParseError : std::runtime_error {
  SourceLocation loc;
  ParseError(SourceLocation l, std::string msg) : std::runtime_error(std::move(msg)), loc(l) {}
};

std::vector<Token> tokenize(std::string_view source);

std::unique_ptr<ast::Expr> parse(std::string_view source);

std::unique_ptr<ast::Expr> parseTokens(const std::vector<Token>& tokens);

}  // namespace mbs
