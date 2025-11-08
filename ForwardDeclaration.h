#pragma once
#include <memory>

struct Expr;
struct Stat;
typedef std::shared_ptr<Expr> ExprPtr;
typedef std::shared_ptr<Stat> StatPtr;