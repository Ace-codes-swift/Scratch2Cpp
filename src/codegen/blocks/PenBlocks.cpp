// PenBlocks.cpp - Scratch pen extension.
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"

namespace s2c::codegen {

void registerPenBlocks(BlockRegistry& r) {
    r.statement("pen_clear", [](const ir::Block&, TargetCompiler&, Emitter& out) {
        out.line("clearPen();");
    });
    r.statement("pen_stamp", [](const ir::Block&, TargetCompiler&, Emitter& out) {
        out.line("stamp();");
    });
    r.statement("pen_penDown", [](const ir::Block&, TargetCompiler&, Emitter& out) { out.line("penDown();"); });
    r.statement("pen_penUp", [](const ir::Block&, TargetCompiler&, Emitter& out) { out.line("penUp();"); });
    // Older projects still use these opcode spellings.
    r.statement("pen_down", [](const ir::Block&, TargetCompiler&, Emitter& out) { out.line("penDown();"); });
    r.statement("pen_up", [](const ir::Block&, TargetCompiler&, Emitter& out) { out.line("penUp();"); });

    r.statement("pen_setPenColorToColor", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("setPenColorToColor(" + c.value(b, "COLOR") + ");");
    });
    r.statement("pen_setPenColourToColour", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("setPenColorToColor(" + c.value(b, "COLOUR") + ");");
    });
    r.statement("pen_changePenColorParamBy", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("changePenColorParam(" + c.value(b, "COLOR_PARAM") + ", " + c.number(b, "VALUE") + ");");
    });
    r.statement("pen_setPenColorParamTo", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("setPenColorParam(" + c.value(b, "COLOR_PARAM") + ", " + c.number(b, "VALUE") + ");");
    });
    r.statement("pen_changePenSizeBy", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("changePenSizeBy(" + c.number(b, "SIZE") + ");");
    });
    r.statement("pen_setPenSizeTo", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("setPenSizeTo(" + c.number(b, "SIZE") + ");");
    });

    // Scratch 2 leftovers that still appear in converted projects.
    r.statement("pen_setPenHueToNumber", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("setPenColorParam(scratch::Value(std::string(\"color\")), " + c.number(b, "HUE") + " / 2.0);");
    });
    r.statement("pen_changePenHueBy", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("changePenColorParam(scratch::Value(std::string(\"color\")), " + c.number(b, "HUE") + " / 2.0);");
    });
    r.statement("pen_setPenShadeToNumber", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("setPenColorParam(scratch::Value(std::string(\"brightness\")), " + c.number(b, "SHADE") + ");");
    });
    r.statement("pen_changePenShadeBy", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("changePenColorParam(scratch::Value(std::string(\"brightness\")), " + c.number(b, "SHADE") + ");");
    });
}

}  // namespace s2c::codegen
