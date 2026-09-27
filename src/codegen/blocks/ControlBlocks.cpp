// ControlBlocks.cpp - "Control" palette.
//
// Loops yield to the scheduler at the end of every iteration, exactly where
// scratch-vm yields, so timing and interleaving of scripts match Scratch.
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"

namespace s2c::codegen {

namespace {
const char* kYield = "co_await scratch::Yield{};";
}

void registerControlBlocks(BlockRegistry& r) {
    r.statement("control_wait", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("co_await wait(" + c.number(b, "DURATION") + ");");
    });
    r.statement("control_repeat", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        const std::string count = c.freshName("repeatCount");
        const std::string i = c.freshName("repeatIndex");
        out.open("");
        out.line("const double " + count + " = std::round(" + c.number(b, "TIMES") + ");");
        out.open("for (double " + i + " = 0; " + i + " < " + count + "; ++" + i + ")");
        c.substack(b, "SUBSTACK", out);
        out.line(kYield);
        out.close();
        out.close();
    });
    r.statement("control_forever", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.open("for (;;)");
        c.substack(b, "SUBSTACK", out);
        out.line(kYield);
        out.close();
    });
    r.statement("control_if", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.open("if (" + c.boolean(b, "CONDITION") + ")");
        c.substack(b, "SUBSTACK", out);
        out.close();
    });
    r.statement("control_if_else", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.open("if (" + c.boolean(b, "CONDITION") + ")");
        c.substack(b, "SUBSTACK", out);
        out.dedent();
        out.line("} else {");
        out.indent();
        c.substack(b, "SUBSTACK2", out);
        out.close();
    });
    r.statement("control_wait_until", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.open("while (!(" + c.boolean(b, "CONDITION") + "))");
        out.line(kYield);
        out.close();
    });
    r.statement("control_repeat_until", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.open("while (!(" + c.boolean(b, "CONDITION") + "))");
        c.substack(b, "SUBSTACK", out);
        out.line(kYield);
        out.close();
    });
    // Obsolete but still executable in Scratch 3.
    r.statement("control_while", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.open("while (" + c.boolean(b, "CONDITION") + ")");
        c.substack(b, "SUBSTACK", out);
        out.line(kYield);
        out.close();
    });
    r.statement("control_for_each", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        const ir::Field* var = b.field("VARIABLE");
        if (!var) { c.unsupportedStatement(b, out, "missing variable"); return; }
        const std::string count = c.freshName("forEachCount");
        const std::string i = c.freshName("forEachIndex");
        out.open("");
        out.line("const double " + count + " = std::round(" + c.number(b, "VALUE") + ");");
        out.open("for (double " + i + " = 1; " + i + " <= " + count + "; ++" + i + ")");
        out.line(c.variableRef(*var) + " = scratch::Value(" + i + ");");
        c.substack(b, "SUBSTACK", out);
        out.line(kYield);
        out.close();
        out.close();
    });
    r.statement("control_stop", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        const std::string option = c.field(b, "STOP_OPTION", "all");
        if (option == "all") {
            out.line("runtime().stopAll();");
            out.line("co_return;");
        } else if (option == "this script") {
            out.line("co_return;");
        } else {  // "other scripts in sprite" / "other scripts in stage"
            out.line("runtime().stopOtherScripts(this, &th);");
        }
    });
    r.statement("control_create_clone_of", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("createCloneOf(" + c.value(b, "CLONE_OPTION") + ");");
    });
    r.statement("control_delete_this_clone", [](const ir::Block&, TargetCompiler& c, Emitter& out) {
        if (c.isStage()) {
            out.line("// control_delete_this_clone has no effect on the stage");
            return;
        }
        out.line("deleteThisClone();");
        out.line("co_return;");
    });
    // Hidden "counter" blocks from Scratch 3's debug palette.
    r.statement("control_clear_counter", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        c.unsupportedStatement(b, out, "hidden debug block");
    });
    r.statement("control_incr_counter", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        c.unsupportedStatement(b, out, "hidden debug block");
    });
    r.expression("control_get_counter", [](const ir::Block& b, TargetCompiler& c) {
        return c.unsupportedExpression(b, "hidden debug block");
    });
}

}  // namespace s2c::codegen
