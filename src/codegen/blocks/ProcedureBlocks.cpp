// ProcedureBlocks.cpp - custom blocks ("My Blocks" palette).
//
// A definition becomes a coroutine member function; a call becomes
// `co_await proc(th, args...)`. Arguments are passed as scratch::Value.
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"

namespace s2c::codegen {

void registerProcedureBlocks(BlockRegistry& r) {
    r.statement("procedures_call", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (!b.mutation) {
            c.unsupportedStatement(b, out, "call without mutation data");
            return;
        }
        const ProcedureInfo* proc = c.procedure(b.mutation->proccode);
        if (!proc) {
            c.diagnostics().warning("Call to undefined custom block \"" + b.mutation->proccode + "\" ignored", c.target().name);
            out.line("// call to undefined custom block \"" + b.mutation->proccode + "\" ignored");
            return;
        }
        std::string args = "th";
        // Arguments are matched by id: the call's inputs are keyed by the
        // argument ids from the prototype.
        for (size_t i = 0; i < proc->argumentIds.size(); ++i) {
            const std::string& argId = proc->argumentIds[i];
            const ir::Input* in = b.input(argId);
            if (in && in->kind == ir::Input::Kind::Empty) in = nullptr;
            if (!in) {
                // Empty boolean slots read as false, empty text slots as "".
                args += ", scratch::Value()";
                continue;
            }
            args += ", " + c.expression(in).asValue();
        }
        out.line("co_await " + proc->cppName + "(" + args + ");");
    });

    r.expression("argument_reporter_string_number", [](const ir::Block& b, TargetCompiler& c) {
        const std::string ref = c.argumentRef(c.field(b, "VALUE"));
        if (ref.empty()) return Expr::value("scratch::Value()");   // outside of a definition
        return Expr::value(ref);
    });
    r.expression("argument_reporter_boolean", [](const ir::Block& b, TargetCompiler& c) {
        const std::string ref = c.argumentRef(c.field(b, "VALUE"));
        if (ref.empty()) return Expr::boolean("false");
        return Expr::boolean(ref + ".toBool()");
    });
    // procedures_definition / procedures_prototype are handled by TargetCompiler.
}

}  // namespace s2c::codegen
