#pragma once

#include "../include/mbs/script.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace mbs {

enum class TokenKind {
  End,
  Ident,
  AtIdent,
  Number,
  String,
  True,
  False,
  Null,
  Plus,
  Minus,
  Star,
  Slash,
  Caret,
  Bang,
  EqEq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  AndAnd,
  OrOr,
  LParen,
  RParen,
  LBracket,
  RBracket,
  LBrace,
  RBrace,
  Comma,
  Dot,
  Colon,
  Percent,
  QuestionQuestion,
  Question,
  In,
};

struct Token {
  TokenKind kind{TokenKind::End};
  std::string_view text;
  double numberValue{0};
  std::string stringValue;  // for String token, owned copy
  SourceLocation start;
};

class Lexer {
 public:
  explicit Lexer(std::string_view src);

  Token next();

 private:
  void skipWhitespace();
  void skipLineComment();
  char peek(std::size_t ahead = 0) const;
  void advance(std::size_t n = 1);
  Token makeToken(TokenKind k, std::size_t len);
  Token lexNumber();
  Token lexString(char quote);
  Token lexIdentOrKeyword();

  std::string_view src_;
  std::size_t pos_{0};
  std::size_t line_{1};
  std::size_t col_{1};
  std::vector<std::string> stringStorage_;  // keep string literals alive
};

std::string tokenKindName(TokenKind k);

}  // namespace mbs
