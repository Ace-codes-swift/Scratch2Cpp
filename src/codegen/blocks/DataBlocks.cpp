// DataBlocks.cpp - "Variables" palette (variables and lists).
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"

namespace s2c::codegen {

namespace {

const ir::Field* variableField(const ir::Block& b, TargetCompiler& c, Emitter* out) {
    const ir::Field* f = b.field("VARIABLE");
    if (!f && out) c.unsupportedStatement(b, *out, "missing VARIABLE field");
    return f;
}

const ir::Field* listField(const ir::Block& b, TargetCompiler& c, Emitter* out) {
    const ir::Field* f = b.field("LIST");
    if (!f && out) c.unsupportedStatement(b, *out, "missing LIST field");
    return f;
}

}  // namespace

void registerDataBlocks(BlockRegistry& r) {
    // ---- Variables -------------------------------------------------------
    r.expression("data_variable", [](const ir::Block& b, TargetCompiler& c) {
        const ir::Field* f = variableField(b, c, nullptr);
        if (!f) return c.unsupportedExpression(b, "missing VARIABLE field");
        return Expr::value(c.variableRef(*f));
    });
    r.statement("data_setvariableto", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (const ir::Field* f = variableField(b, c, &out)) {
            out.line(c.variableRef(*f) + " = " + c.value(b, "VALUE") + ";");
        }
    });
    r.statement("data_changevariableby", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (const ir::Field* f = variableField(b, c, &out)) {
            const std::string ref = c.variableRef(*f);
            out.line(ref + " = scratch::Value(" + ref + ".toNumber() + " + c.number(b, "VALUE") + ");");
        }
    });
    r.statement("data_showvariable", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("showVariable(" + c.fieldLiteral(b, "VARIABLE") + ");");
    });
    r.statement("data_hidevariable", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("hideVariable(" + c.fieldLiteral(b, "VARIABLE") + ");");
    });

    // ---- Lists -------------------------------------------------------------
    r.expression("data_listcontents", [](const ir::Block& b, TargetCompiler& c) {
        const ir::Field* f = listField(b, c, nullptr);
        if (!f) return c.unsupportedExpression(b, "missing LIST field");
        return Expr::string(c.listRef(*f) + ".contents()");
    });
    r.statement("data_addtolist", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (const ir::Field* f = listField(b, c, &out)) out.line(c.listRef(*f) + ".add(" + c.value(b, "ITEM") + ");");
    });
    r.statement("data_deleteoflist", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (const ir::Field* f = listField(b, c, &out)) out.line(c.listRef(*f) + ".deleteAt(" + c.value(b, "INDEX") + ");");
    });
    r.statement("data_deletealloflist", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (const ir::Field* f = listField(b, c, &out)) out.line(c.listRef(*f) + ".clear();");
    });
    r.statement("data_insertatlist", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (const ir::Field* f = listField(b, c, &out)) {
            out.line(c.listRef(*f) + ".insertAt(" + c.value(b, "INDEX") + ", " + c.value(b, "ITEM") + ");");
        }
    });
    r.statement("data_replaceitemoflist", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (const ir::Field* f = listField(b, c, &out)) {
            out.line(c.listRef(*f) + ".replaceAt(" + c.value(b, "INDEX") + ", " + c.value(b, "ITEM") + ");");
        }
    });
    r.expression("data_itemoflist", [](const ir::Block& b, TargetCompiler& c) {
        const ir::Field* f = listField(b, c, nullptr);
        if (!f) return c.unsupportedExpression(b, "missing LIST field");
        return Expr::value(c.listRef(*f) + ".itemAt(" + c.value(b, "INDEX") + ")");
    });
    r.expression("data_itemnumoflist", [](const ir::Block& b, TargetCompiler& c) {
        const ir::Field* f = listField(b, c, nullptr);
        if (!f) return c.unsupportedExpression(b, "missing LIST field");
        return Expr::number(c.listRef(*f) + ".indexOf(" + c.value(b, "ITEM") + ")");
    });
    r.expression("data_lengthoflist", [](const ir::Block& b, TargetCompiler& c) {
        const ir::Field* f = listField(b, c, nullptr);
        if (!f) return c.unsupportedExpression(b, "missing LIST field");
        return Expr::number("static_cast<double>(" + c.listRef(*f) + ".length())");
    });
    r.expression("data_listcontainsitem", [](const ir::Block& b, TargetCompiler& c) {
        const ir::Field* f = listField(b, c, nullptr);
        if (!f) return c.unsupportedExpression(b, "missing LIST field");
        return Expr::boolean(c.listRef(*f) + ".contains(" + c.value(b, "ITEM") + ")");
    });
    r.statement("data_showlist", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("showList(" + c.fieldLiteral(b, "LIST") + ");");
    });
    r.statement("data_hidelist", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("hideList(" + c.fieldLiteral(b, "LIST") + ");");
    });
}

}  // namespace s2c::codegen
