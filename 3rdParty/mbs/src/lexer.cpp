#include "lexer.hpp"
#include "parser.hpp"

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>

namespace mbs {

Lexer::Lexer(const std::string_view src) : src_(src) {}

char Lexer::peek(const std::size_t ahead) const {
  if (pos_ + ahead >= src_.size()) {
    return '\0';
  }
  return src_[pos_ + ahead];
}

void Lexer::advance(const std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    if (pos_ >= src_.size()) {
      return;
    }
    if (src_[pos_] == '\n') {
      ++line_;
      col_ = 1;
    } else {
      ++col_;
    }
    ++pos_;
  }
}

void Lexer::skipWhitespace() {
  while (true) {
    char c = peek();
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance(1);
    } else {
      break;
    }
  }
}

void Lexer::skipLineComment() {
  // Assumes current char is second '/' of '//'
  while (peek() != '\0' && peek() != '\n') {
    advance(1);
  }
}

Token Lexer::makeToken(const TokenKind k, const std::size_t len) {
  const SourceLocation start{line_, col_};
  const std::string_view text = src_.substr(pos_, len);
  advance(len);
  return Token{k, text, 0, {}, start};
}

Token Lexer::lexNumber() {
  const SourceLocation start{line_, col_};
  const std::size_t begin = pos_;
  while (std::isdigit(static_cast<unsigned char>(peek()))) {
    advance(1);
  }
  if (peek() == '.') {
    advance(1);
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
      advance(1);
    }
  }
  if (peek() == 'e' || peek() == 'E') {
    const std::size_t saved_pos = pos_;
    const std::size_t saved_line = line_;
    const std::size_t saved_col = col_;
    advance(1);
    if (peek() == '+' || peek() == '-') {
      advance(1);
    }
    if (!std::isdigit(static_cast<unsigned char>(peek()))) {
      pos_ = saved_pos;
      line_ = saved_line;
      col_ = saved_col;
    } else {
      while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance(1);
      }
    }
  }
  const std::string_view numText = src_.substr(begin, pos_ - begin);
  double v = 0;
  try {
    v = std::stod(std::string(numText));
  } catch (const std::out_of_range&) {
    throw ParseError(start, "number literal out of range");
  }
  Token t{TokenKind::Number, numText, v, {}, start};
  return t;
}

Token Lexer::lexString(const char quote) {
  const SourceLocation start{line_, col_};
  advance(1);  // opening quote
  std::string out;

  while (peek() != '\0' && peek() != quote) {
    if (peek() == '\\') {
      advance(1);

      const char e = peek();
      if (e == '\0') {
        break;
      }

      switch (e) {
        case 'n':
          out += '\n';
          break;
        case 't':
          out += '\t';
          break;
        case 'r':
          out += '\r';
          break;
        case '\\':
          out += '\\';
          break;
        case '\'':
          out += '\'';
          break;
        case '"':
          out += '"';
          break;
        default:
          out += e;
          break;
      }
      advance(1);
    } else if (peek() == '\n') {
      advance(1);
      out += '\n';
    } else {
      out += peek();
      advance(1);
    }
  }

  if (peek() != quote) {
    throw ParseError(start, "unterminated string");
  }

  advance(1);  // closing quote
  stringStorage_.push_back(std::move(out));

  Token t;
  t.kind = TokenKind::String;
  t.text = {};
  t.stringValue = stringStorage_.back();
  t.start = start;

  return t;
}

Token Lexer::lexIdentOrKeyword() {
  const SourceLocation start{line_, col_};
  const std::size_t begin = pos_;
  while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
    advance(1);
  }

  const std::string_view id = src_.substr(begin, pos_ - begin);
  if (id == "true") {
    return Token{TokenKind::True, id, 0, {}, start};
  }

  if (id == "false") {
    return Token{TokenKind::False, id, 0, {}, start};
  }

  if (id == "null") {
    return Token{TokenKind::Null, id, 0, {}, start};
  }

  if (id == "in") {
    return Token{TokenKind::In, id, 0, {}, start};
  }

  return Token{TokenKind::Ident, id, 0, {}, start};
}

Token Lexer::next() {
  while (true) {
    skipWhitespace();
    if (pos_ >= src_.size()) {
      return Token{
        TokenKind::End,
        src_.substr(pos_, 0),
        0,
        {},
        {line_, col_}
      };
    }

    const char c = peek();
    const SourceLocation start{line_, col_};

    if (c == '/' && peek(1) == '/') {
      advance(2);
      skipLineComment();
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(c))
      || (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))) {
      return lexNumber();
    }

    if (c == '"' || c == '\'') {
      return lexString(c);
    }

    if (c == '@') {
      advance(1);
      if (!std::isalpha(static_cast<unsigned char>(peek())) && peek() != '_') {
        throw ParseError(start, "expected identifier after @");
      }

      const Token id = lexIdentOrKeyword();
      if (id.kind != TokenKind::Ident) {
        throw ParseError(start, "invalid token after @");
      }

      Token t{TokenKind::AtIdent, id.text, 0, {}, start};
      return t;
    }

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      return lexIdentOrKeyword();
    }

    switch (c) {
      case '+':
        return makeToken(TokenKind::Plus, 1);
      case '-':
        return makeToken(TokenKind::Minus, 1);
      case '*':
        return makeToken(TokenKind::Star, 1);
      case '/':
        return makeToken(TokenKind::Slash, 1);
      case '^':
        return makeToken(TokenKind::Caret, 1);
      case '(':
        return makeToken(TokenKind::LParen, 1);
      case ')':
        return makeToken(TokenKind::RParen, 1);
      case '[':
        return makeToken(TokenKind::LBracket, 1);
      case ']':
        return makeToken(TokenKind::RBracket, 1);
      case '{':
        return makeToken(TokenKind::LBrace, 1);
      case '}':
        return makeToken(TokenKind::RBrace, 1);
      case ',':
        return makeToken(TokenKind::Comma, 1);
      case ':':
        return makeToken(TokenKind::Colon, 1);
      case '.':
        return makeToken(TokenKind::Dot, 1);
      case '%':
        return makeToken(TokenKind::Percent, 1);
      case '?':
        if (peek(1) == '?') {
          return makeToken(TokenKind::QuestionQuestion, 2);
        }
        return makeToken(TokenKind::Question, 1);
      case '!':
        if (peek(1) == '=') {
          return makeToken(TokenKind::Ne, 2);
        }
        return makeToken(TokenKind::Bang, 1);
      case '=':
        if (peek(1) == '=') {
          return makeToken(TokenKind::EqEq, 2);
        }
        throw ParseError(start, "unexpected '='");
      case '<':
        if (peek(1) == '=') {
          return makeToken(TokenKind::Le, 2);
        }
        return makeToken(TokenKind::Lt, 1);
      case '>':
        if (peek(1) == '=') {
          return makeToken(TokenKind::Ge, 2);
        }
        return makeToken(TokenKind::Gt, 1);
      case '&':
        if (peek(1) == '&') {
          return makeToken(TokenKind::AndAnd, 2);
        }
        throw ParseError(start, "unexpected '&'");
      case '|':
        if (peek(1) == '|') {
          return makeToken(TokenKind::OrOr, 2);
        }
        throw ParseError(start, "unexpected '|'");
      default:
        throw ParseError(start, "unexpected character");
    }
  }
}

std::string tokenKindName(const TokenKind k) {
  switch (k) {
    case TokenKind::End:
      return "End";
    case TokenKind::Ident:
      return "Ident";
    case TokenKind::AtIdent:
      return "AtIdent";
    case TokenKind::Number:
      return "Number";
    case TokenKind::String:
      return "String";
    case TokenKind::True:
      return "True";
    case TokenKind::False:
      return "False";
    case TokenKind::Null:
      return "Null";
    default:
      return "Op";
  }
}

}  // namespace mbs
