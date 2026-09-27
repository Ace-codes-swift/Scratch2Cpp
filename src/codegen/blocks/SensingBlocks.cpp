// SensingBlocks.cpp - "Sensing" palette (mouse/keyboard via SDL3 in the runtime).
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"
#include "utils/StringUtils.hpp"

namespace s2c::codegen {

void registerSensingBlocks(BlockRegistry& r) {
    r.expression("sensing_touchingobject", [](const ir::Block& b, TargetCompiler& c) {
        if (c.isStage()) return Expr::boolean("false");
        return Expr::boolean("touching(" + c.value(b, "TOUCHINGOBJECTMENU") + ")");
    });
    r.expression("sensing_touchingcolor", [](const ir::Block& b, TargetCompiler& c) {
        if (c.isStage()) return Expr::boolean("false");
        return Expr::boolean("touchingColor(" + c.value(b, "COLOR") + ")");
    });
    r.expression("sensing_coloristouchingcolor", [](const ir::Block& b, TargetCompiler& c) {
        if (c.isStage()) return Expr::boolean("false");
        return Expr::boolean("colorIsTouchingColor(" + c.value(b, "COLOR") + ", " + c.value(b, "COLOR2") + ")");
    });
    r.expression("sensing_distanceto", [](const ir::Block& b, TargetCompiler& c) {
        if (c.isStage()) return Expr::number("10000.0");
        return Expr::number("distanceTo(" + c.value(b, "DISTANCETOMENU") + ")");
    });
    r.statement("sensing_askandwait", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        c.unsupportedStatement(b, out, "text input dialog is not implemented");
    });
    r.expression("sensing_answer", [](const ir::Block&, TargetCompiler&) {
        return Expr::string("std::string()");
    });
    r.expression("sensing_keypressed", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::boolean("keyPressed(" + c.value(b, "KEY_OPTION") + ")");
    });
    r.expression("sensing_mousedown", [](const ir::Block&, TargetCompiler&) { return Expr::boolean("mouseDown()"); });
    r.expression("sensing_mousex", [](const ir::Block&, TargetCompiler&) { return Expr::number("mouseX()"); });
    r.expression("sensing_mousey", [](const ir::Block&, TargetCompiler&) { return Expr::number("mouseY()"); });
    r.statement("sensing_setdragmode", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (c.isStage()) return;
        out.line(std::string("setDraggable(") + (c.field(b, "DRAG_MODE", "draggable") == "draggable" ? "true" : "false") +
                 ");   // note: mouse dragging of sprites is not implemented");
    });
    r.expression("sensing_loudness", [](const ir::Block& b, TargetCompiler& c) {
        c.diagnostics().unsupportedBlock(b.opcode, c.target().name, "microphone input is not available; reports -1");
        return Expr::number("loudness()");
    });
    r.expression("sensing_timer", [](const ir::Block&, TargetCompiler&) { return Expr::number("timer()"); });
    r.statement("sensing_resettimer", [](const ir::Block&, TargetCompiler&, Emitter& out) { out.line("resetTimer();"); });
    r.expression("sensing_of", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::value("sensingOf(" + c.fieldLiteral(b, "PROPERTY", "x position") + ", " + c.value(b, "OBJECT") + ")");
    });
    r.expression("sensing_current", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::value("current(" + str::cppStringLiteral(str::toLower(c.field(b, "CURRENTMENU", "YEAR")) == "dayofweek"
                                                              ? "DAYOFWEEK" : c.field(b, "CURRENTMENU", "YEAR")) + ")");
    });
    r.expression("sensing_dayssince2000", [](const ir::Block&, TargetCompiler&) { return Expr::number("daysSince2000()"); });
    r.expression("sensing_username", [](const ir::Block&, TargetCompiler&) { return Expr::string("username()"); });
    // Video sensing extension.
    r.statement("videoSensing_videoToggle", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        c.unsupportedStatement(b, out, "video sensing extension");
    });
    r.statement("videoSensing_setVideoTransparency", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        c.unsupportedStatement(b, out, "video sensing extension");
    });
    r.expression("videoSensing_videoOn", [](const ir::Block& b, TargetCompiler& c) {
        return c.unsupportedExpression(b, "video sensing extension");
    });
}

}  // namespace s2c::codegen
