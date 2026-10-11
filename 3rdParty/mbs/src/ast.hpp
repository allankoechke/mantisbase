#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace mbs::ast {
    enum class BinOp {
        Add,
        Sub,
        Mul,
        Div,
        Mod,
        Pow,
        Eq,
        Ne,
        Lt,
        Le,
        Gt,
        Ge,
        And,
        Or,
        NullCoalesce,
        In,
    };

    enum class UnOp { Not, Neg };

    struct Expr;

    struct Segment {
        enum class Kind { Member, Method, Index } kind{Kind::Member};

        std::string name; // Member / Method
        std::vector<std::unique_ptr<Expr> > args;
        std::unique_ptr<Expr> indexExpr;
    };

    struct Expr {
        enum class Kind { Literal, Unary, Binary, Ternary, Chain, Array, Object, ContextVar } kind{Kind::Literal};

        nlohmann::json literal; // Literal
        std::string ctxName; // ContextVar: name after @

        UnOp unOp{};
        std::unique_ptr<Expr> child; // Unary

        BinOp binOp{};
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right; // Binary

        std::unique_ptr<Expr> elseExpr; // Ternary (condition=left, then=right, else=elseExpr)

        std::unique_ptr<Expr> chainBase;
        std::vector<Segment> segments; // Chain

        std::vector<std::unique_ptr<Expr> > arrayElems; // Array

        struct ObjPair {
            std::string key;
            std::unique_ptr<Expr> value;
        };

        std::vector<ObjPair> objectPairs; // Object
    };
} // namespace mbs::ast
