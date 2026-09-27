// MotionBlocks.cpp - "Motion" palette -> scratch::Sprite motion API.
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"

namespace s2c::codegen {

namespace {

// Motion blocks do nothing on the stage (Scratch does not even offer them).
bool spriteOnly(const ir::Block& block, TargetCompiler& c, Emitter& out) {
    if (c.isSprite()) return true;
    out.line("// " + block.opcode + " has no effect on the stage");
    return false;
}

}  // namespace

void registerMotionBlocks(BlockRegistry& r) {
    r.statement("motion_movesteps", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("moveSteps(" + c.number(b, "STEPS") + ");");
    });
    r.statement("motion_turnright", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("turnRight(" + c.number(b, "DEGREES") + ");");
    });
    r.statement("motion_turnleft", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("turnLeft(" + c.number(b, "DEGREES") + ");");
    });
    r.statement("motion_goto", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("goTo(" + c.value(b, "TO") + ");");
    });
    r.statement("motion_gotoxy", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("goToXY(" + c.number(b, "X") + ", " + c.number(b, "Y") + ");");
    });
    r.statement("motion_glidesecstoxy", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) {
            out.line("co_await glideTo(" + c.number(b, "SECS") + ", " + c.number(b, "X") + ", " + c.number(b, "Y") + ");");
        }
    });
    r.statement("motion_glideto", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("co_await glideToTarget(" + c.number(b, "SECS") + ", " + c.value(b, "TO") + ");");
    });
    r.statement("motion_pointindirection", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("pointInDirection(" + c.number(b, "DIRECTION") + ");");
    });
    r.statement("motion_pointtowards", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("pointTowards(" + c.value(b, "TOWARDS") + ");");
    });
    r.statement("motion_changexby", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("changeX(" + c.number(b, "DX") + ");");
    });
    r.statement("motion_setx", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("setX(" + c.number(b, "X") + ");");
    });
    r.statement("motion_changeyby", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("changeY(" + c.number(b, "DY") + ");");
    });
    r.statement("motion_sety", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("setY(" + c.number(b, "Y") + ");");
    });
    r.statement("motion_ifonedgebounce", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("ifOnEdgeBounce();");
    });
    r.statement("motion_setrotationstyle", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) {
            out.line("setRotationStyle(scratch::Value(" + c.fieldLiteral(b, "STYLE", "all around") + "));");
        }
    });

    r.expression("motion_xposition", [](const ir::Block&, TargetCompiler& c) {
        return c.isSprite() ? Expr::number("x()") : Expr::number("0.0");
    });
    r.expression("motion_yposition", [](const ir::Block&, TargetCompiler& c) {
        return c.isSprite() ? Expr::number("y()") : Expr::number("0.0");
    });
    r.expression("motion_direction", [](const ir::Block&, TargetCompiler& c) {
        return c.isSprite() ? Expr::number("direction()") : Expr::number("90.0");
    });
}

}  // namespace s2c::codegen
