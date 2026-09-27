// OperatorBlocks.cpp - "Operators" palette -> scratch::ops helpers.
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"
#include "utils/StringUtils.hpp"

namespace s2c::codegen {

void registerOperatorBlocks(BlockRegistry& r) {
    auto arith = [&r](const char* opcode, const char* fn) {
        r.expression(opcode, [fn](const ir::Block& b, TargetCompiler& c) {
            return Expr::number(std::string("scratch::ops::") + fn + "(" + c.number(b, "NUM1") + ", " + c.number(b, "NUM2") + ")");
        });
    };
    arith("operator_add", "add");
    arith("operator_subtract", "sub");
    arith("operator_multiply", "mul");
    arith("operator_divide", "div");
    arith("operator_mod", "mod");

    r.expression("operator_random", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::number("scratch::ops::random(" + c.value(b, "FROM") + ", " + c.value(b, "TO") + ")");
    });

    auto compare = [&r](const char* opcode, const char* fn) {
        r.expression(opcode, [fn](const ir::Block& b, TargetCompiler& c) {
            return Expr::boolean(std::string("scratch::ops::") + fn + "(" + c.value(b, "OPERAND1") + ", " + c.value(b, "OPERAND2") + ")");
        });
    };
    compare("operator_gt", "gt");
    compare("operator_lt", "lt");
    compare("operator_equals", "eq");

    r.expression("operator_and", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::boolean("(" + c.boolean(b, "OPERAND1") + " && " + c.boolean(b, "OPERAND2") + ")");
    });
    r.expression("operator_or", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::boolean("(" + c.boolean(b, "OPERAND1") + " || " + c.boolean(b, "OPERAND2") + ")");
    });
    r.expression("operator_not", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::boolean("!(" + c.boolean(b, "OPERAND") + ")");
    });

    r.expression("operator_join", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::string("scratch::ops::join(" + c.value(b, "STRING1") + ", " + c.value(b, "STRING2") + ")");
    });
    r.expression("operator_letter_of", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::string("scratch::ops::letterOf(" + c.value(b, "LETTER") + ", " + c.value(b, "STRING") + ")");
    });
    r.expression("operator_length", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::number("scratch::ops::length(" + c.value(b, "STRING") + ")");
    });
    r.expression("operator_contains", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::boolean("scratch::ops::contains(" + c.value(b, "STRING1") + ", " + c.value(b, "STRING2") + ")");
    });
    r.expression("operator_round", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::number("scratch::ops::round(" + c.number(b, "NUM") + ")");
    });
    r.expression("operator_mathop", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::number("scratch::ops::mathop(" + c.fieldLiteral(b, "OPERATOR", "abs") + ", " + c.number(b, "NUM") + ")");
    });
}

}  // namespace s2c::codegen
