#include "parser.hpp"

#include <utility>

namespace mbs {

inline constexpr int MAX_PARSE_DEPTH = 512;

std::vector<Token> tokenize(std::string_view source) {
  Lexer lex(source);
  std::vector<Token> out;
  while (true) {
    Token t = lex.next();
    out.push_back(std::move(t));
    if (out.back().kind == TokenKind::End) {
      break;
    }
  }
  return out;
}

namespace {

class Parser {
 public:
  explicit Parser(const std::vector<Token>& tok) : tok_(tok) {}

  std::unique_ptr<ast::Expr> parseExpr() {
    ParseDepthGuard guard(depth_, *this);
    return parseTernary();
  }

  std::unique_ptr<ast::Expr> parseTernary() {
    auto cond = parseNullCoalesce();
    if (peek().kind == TokenKind::Question) {
      consume();
      auto thenExpr = parseExpr();
      expect(TokenKind::Colon, "':' in ternary expression");
      auto elseExpr = parseExpr();
      auto n = std::make_unique<ast::Expr>();
      n->kind = ast::Expr::Kind::Ternary;
      n->left = std::move(cond);
      n->right = std::move(thenExpr);
      n->elseExpr = std::move(elseExpr);
      return n;
    }
    return cond;
  }

  std::unique_ptr<ast::Expr> parseNullCoalesce() {
    auto left = parseOr();
    while (peek().kind == TokenKind::QuestionQuestion) {
      consume();
      auto right = parseOr();
      left = makeBinary(ast::BinOp::NullCoalesce, std::move(left), std::move(right));
    }
    return left;
  }

  void expectEnd() {
    if (peek().kind != TokenKind::End) {
      error("trailing tokens after expression");
    }
  }

 private:
  const std::vector<Token>& tok_;
  std::size_t i_{0};
  int depth_{0};

  struct ParseDepthGuard {
    int& d;
    ParseDepthGuard(int& depth, const Parser& p) : d(depth) {
      if (++d > MAX_PARSE_DEPTH) {
        --d;
        const Token& t = p.peek();
        throw ParseError(t.start, "expression too deeply nested (limit 512)");
      }
    }
    ~ParseDepthGuard() { --d; }
    ParseDepthGuard(const ParseDepthGuard&) = delete;
    ParseDepthGuard& operator=(const ParseDepthGuard&) = delete;
  };

  const Token& peek() const { return tok_.at(i_); }
  Token consume() { return tok_.at(i_++); }

  [[noreturn]] void error(std::string msg) {
    const Token& t = peek();
    throw ParseError(t.start, std::move(msg));
  }

  void expect(TokenKind k, const char* what) {
    if (peek().kind != k) {
      error(std::string("expected ") + what);
    }
    consume();
  }

  std::unique_ptr<ast::Expr> parseOr() {
    auto left = parseAnd();
    while (peek().kind == TokenKind::OrOr) {
      consume();
      auto right = parseAnd();
      auto n = std::make_unique<ast::Expr>();
      n->kind = ast::Expr::Kind::Binary;
      n->binOp = ast::BinOp::Or;
      n->left = std::move(left);
      n->right = std::move(right);
      left = std::move(n);
    }
    return left;
  }

  std::unique_ptr<ast::Expr> parseAnd() {
    auto left = parseEquality();
    while (peek().kind == TokenKind::AndAnd) {
      consume();
      auto right = parseEquality();
      auto n = std::make_unique<ast::Expr>();
      n->kind = ast::Expr::Kind::Binary;
      n->binOp = ast::BinOp::And;
      n->left = std::move(left);
      n->right = std::move(right);
      left = std::move(n);
    }
    return left;
  }

  std::unique_ptr<ast::Expr> parseEquality() {
    auto left = parseComparison();
    while (true) {
      if (peek().kind == TokenKind::EqEq) {
        consume();
        auto right = parseComparison();
        left = makeBinary(ast::BinOp::Eq, std::move(left), std::move(right));
      } else if (peek().kind == TokenKind::Ne) {
        consume();
        auto right = parseComparison();
        left = makeBinary(ast::BinOp::Ne, std::move(left), std::move(right));
      } else {
        break;
      }
    }
    return left;
  }

  std::unique_ptr<ast::Expr> parseComparison() {
    auto left = parseAdditive();
    while (true) {
      ast::BinOp op;
      if (peek().kind == TokenKind::Lt) {
        consume();
        op = ast::BinOp::Lt;
      } else if (peek().kind == TokenKind::Le) {
        consume();
        op = ast::BinOp::Le;
      } else if (peek().kind == TokenKind::Gt) {
        consume();
        op = ast::BinOp::Gt;
      } else if (peek().kind == TokenKind::Ge) {
        consume();
        op = ast::BinOp::Ge;
      } else if (peek().kind == TokenKind::In) {
        consume();
        op = ast::BinOp::In;
      } else {
        break;
      }
      auto right = parseAdditive();
      left = makeBinary(op, std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<ast::Expr> parseAdditive() {
    auto left = parseMultiplicative();
    while (true) {
      if (peek().kind == TokenKind::Plus) {
        consume();
        auto right = parseMultiplicative();
        left = makeBinary(ast::BinOp::Add, std::move(left), std::move(right));
      } else if (peek().kind == TokenKind::Minus) {
        consume();
        auto right = parseMultiplicative();
        left = makeBinary(ast::BinOp::Sub, std::move(left), std::move(right));
      } else {
        break;
      }
    }
    return left;
  }

  std::unique_ptr<ast::Expr> parseMultiplicative() {
    auto left = parsePower();
    while (true) {
      if (peek().kind == TokenKind::Star) {
        consume();
        auto right = parsePower();
        left = makeBinary(ast::BinOp::Mul, std::move(left), std::move(right));
      } else if (peek().kind == TokenKind::Slash) {
        consume();
        auto right = parsePower();
        left = makeBinary(ast::BinOp::Div, std::move(left), std::move(right));
      } else if (peek().kind == TokenKind::Percent) {
        consume();
        auto right = parsePower();
        left = makeBinary(ast::BinOp::Mod, std::move(left), std::move(right));
      } else {
        break;
      }
    }
    return left;
  }

  std::unique_ptr<ast::Expr> parsePower() {
    auto left = parseUnary();
    if (peek().kind == TokenKind::Caret) {
      consume();
      auto right = parsePower();  // right-associative
      return makeBinary(ast::BinOp::Pow, std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<ast::Expr> parseUnary() {
    ParseDepthGuard guard(depth_, *this);
    if (peek().kind == TokenKind::Bang) {
      consume();
      auto c = parseUnary();
      auto n = std::make_unique<ast::Expr>();
      n->kind = ast::Expr::Kind::Unary;
      n->unOp = ast::UnOp::Not;
      n->child = std::move(c);
      return n;
    }
    if (peek().kind == TokenKind::Minus) {
      consume();
      auto c = parseUnary();
      auto n = std::make_unique<ast::Expr>();
      n->kind = ast::Expr::Kind::Unary;
      n->unOp = ast::UnOp::Neg;
      n->child = std::move(c);
      return n;
    }
    return parsePostfix();
  }

  std::unique_ptr<ast::Expr> parsePostfix() {
    auto e = parsePrimary();
    return parseChain(std::move(e));
  }

  std::unique_ptr<ast::Expr> parseChain(std::unique_ptr<ast::Expr> base) {
    while (true) {
      if (peek().kind == TokenKind::Dot) {
        consume();
        if (peek().kind != TokenKind::Ident) {
          error("expected identifier after '.'");
        }
        std::string name(peek().text);
        consume();
        if (peek().kind == TokenKind::LParen) {
          consume();
          ast::Segment seg;
          seg.kind = ast::Segment::Kind::Method;
          seg.name = std::move(name);
          if (peek().kind != TokenKind::RParen) {
            while (true) {
              seg.args.push_back(parseExpr());
              if (peek().kind == TokenKind::Comma) {
                consume();
                continue;
              }
              break;
            }
          }
          expect(TokenKind::RParen, "')'");
          base = appendSegment(std::move(base), std::move(seg));
        } else {
          ast::Segment seg;
          seg.kind = ast::Segment::Kind::Member;
          seg.name = std::move(name);
          base = appendSegment(std::move(base), std::move(seg));
        }
      } else if (peek().kind == TokenKind::LBracket) {
        consume();
        auto ix = parseExpr();
        expect(TokenKind::RBracket, "']'");
        ast::Segment seg;
        seg.kind = ast::Segment::Kind::Index;
        seg.indexExpr = std::move(ix);
        base = appendSegment(std::move(base), std::move(seg));
      } else {
        break;
      }
    }
    return base;
  }

  static std::unique_ptr<ast::Expr> appendSegment(std::unique_ptr<ast::Expr> base, ast::Segment seg) {
    if (base->kind == ast::Expr::Kind::Chain) {
      base->segments.push_back(std::move(seg));
      return base;
    }
    auto ch = std::make_unique<ast::Expr>();
    ch->kind = ast::Expr::Kind::Chain;
    ch->chainBase = std::move(base);
    ch->segments.push_back(std::move(seg));
    return ch;
  }

  std::unique_ptr<ast::Expr> parsePrimary() {
    ParseDepthGuard guard(depth_, *this);
    switch (peek().kind) {
      case TokenKind::Null: {
        consume();
        auto n = std::make_unique<ast::Expr>();
        n->kind = ast::Expr::Kind::Literal;
        n->literal = nullptr;
        return n;
      }
      case TokenKind::True: {
        consume();
        auto n = std::make_unique<ast::Expr>();
        n->kind = ast::Expr::Kind::Literal;
        n->literal = true;
        return n;
      }
      case TokenKind::False: {
        consume();
        auto n = std::make_unique<ast::Expr>();
        n->kind = ast::Expr::Kind::Literal;
        n->literal = false;
        return n;
      }
      case TokenKind::Number: {
        Token t = consume();
        auto n = std::make_unique<ast::Expr>();
        n->kind = ast::Expr::Kind::Literal;
        n->literal = t.numberValue;
        return n;
      }
      case TokenKind::String: {
        Token t = consume();
        auto n = std::make_unique<ast::Expr>();
        n->kind = ast::Expr::Kind::Literal;
        n->literal = t.stringValue;
        return n;
      }
      case TokenKind::AtIdent: {
        Token t = consume();
        auto n = std::make_unique<ast::Expr>();
        n->kind = ast::Expr::Kind::ContextVar;
        n->ctxName = std::string(t.text);
        return n;
      }
      case TokenKind::Ident:
        error("bare identifiers are not allowed; use @name for context variables");
        break;
      case TokenKind::LParen: {
        consume();
        auto e = parseExpr();
        expect(TokenKind::RParen, "')'");
        return e;
      }
      case TokenKind::LBracket: {
        consume();
        auto n = std::make_unique<ast::Expr>();
        n->kind = ast::Expr::Kind::Array;
        if (peek().kind != TokenKind::RBracket) {
          while (true) {
            n->arrayElems.push_back(parseExpr());
            if (peek().kind == TokenKind::Comma) {
              consume();
              continue;
            }
            break;
          }
        }
        expect(TokenKind::RBracket, "']'");
        return n;
      }
      case TokenKind::LBrace: {
        consume();
        auto n = std::make_unique<ast::Expr>();
        n->kind = ast::Expr::Kind::Object;
        if (peek().kind != TokenKind::RBrace) {
          while (true) {
            std::string key;
            if (peek().kind == TokenKind::String) {
              key = std::string(consume().stringValue);
            } else if (peek().kind == TokenKind::Ident) {
              key = std::string(consume().text);
            } else {
              error("expected string or identifier as object key");
            }
            expect(TokenKind::Colon, "':'");
            auto val = parseExpr();
            n->objectPairs.push_back(ast::Expr::ObjPair{std::move(key), std::move(val)});
            if (peek().kind == TokenKind::Comma) {
              consume();
              continue;
            }
            break;
          }
        }
        expect(TokenKind::RBrace, "'}'");
        return n;
      }
      default:
        error("unexpected token in expression");
    }
  }

  static std::unique_ptr<ast::Expr> makeBinary(ast::BinOp op, std::unique_ptr<ast::Expr> l,
                                               std::unique_ptr<ast::Expr> r) {
    auto n = std::make_unique<ast::Expr>();
    n->kind = ast::Expr::Kind::Binary;
    n->binOp = op;
    n->left = std::move(l);
    n->right = std::move(r);
    return n;
  }
};

std::unique_ptr<ast::Expr> parseTokensImpl(const std::vector<Token>& tokens) {
  Parser p(tokens);
  auto e = p.parseExpr();
  p.expectEnd();
  return e;
}

}  // namespace

std::unique_ptr<ast::Expr> parseTokens(const std::vector<Token>& tokens) {
  return parseTokensImpl(tokens);
}

std::unique_ptr<ast::Expr> parse(std::string_view source) {
  auto tok = tokenize(source);
  return parseTokens(tok);
}

}  // namespace mbs
