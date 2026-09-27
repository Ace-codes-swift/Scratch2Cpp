// EventBlocks.cpp - "Events" palette (hat blocks are handled by TargetCompiler).
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"

namespace s2c::codegen {

void registerEventBlocks(BlockRegistry& r) {
    r.statement("event_broadcast", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("broadcast(" + c.value(b, "BROADCAST_INPUT") + ");");
    });
    r.statement("event_broadcastandwait", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("co_await broadcastAndWait(" + c.value(b, "BROADCAST_INPUT") + ");");
    });
    // The broadcast drop-down is a menu shadow; when it is not compacted into
    // a primitive it appears as this block.
    r.expression("event_broadcast_menu", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::string("std::string(" + c.fieldLiteral(b, "BROADCAST_OPTION") + ")");
    });
}

}  // namespace s2c::codegen
