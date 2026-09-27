// LooksBlocks.cpp - "Looks" palette.
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"
#include "utils/StringUtils.hpp"

namespace s2c::codegen {

namespace {

bool spriteOnly(const ir::Block& block, TargetCompiler& c, Emitter& out) {
    if (c.isSprite()) return true;
    out.line("// " + block.opcode + " has no effect on the stage");
    return false;
}

// Only "ghost" and "brightness" can be rendered with plain SDL texture
// modulation; the others are accepted but have no visual effect.
void checkEffect(const ir::Block& b, TargetCompiler& c) {
    const std::string effect = str::toLower(c.field(b, "EFFECT", "color"));
    if (effect != "ghost" && effect != "brightness") {
        c.diagnostics().unsupportedFeature("graphic effect \"" + effect + "\" (not rendered; only ghost/brightness are)", c.target().name);
    }
}

}  // namespace

void registerLooksBlocks(BlockRegistry& r) {
    r.statement("looks_say", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("say(" + c.value(b, "MESSAGE") + ");");
    });
    r.statement("looks_sayforsecs", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("co_await sayForSecs(" + c.value(b, "MESSAGE") + ", " + c.number(b, "SECS") + ");");
    });
    r.statement("looks_think", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("think(" + c.value(b, "MESSAGE") + ");");
    });
    r.statement("looks_thinkforsecs", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("co_await thinkForSecs(" + c.value(b, "MESSAGE") + ", " + c.number(b, "SECS") + ");");
    });
    r.statement("looks_switchcostumeto", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("switchCostumeTo(" + c.value(b, "COSTUME") + ");");
    });
    r.statement("looks_nextcostume", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("nextCostume();");
    });
    r.statement("looks_switchbackdropto", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("switchBackdropTo(" + c.value(b, "BACKDROP") + ");");
    });
    r.statement("looks_switchbackdroptoandwait", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("co_await switchBackdropToAndWait(" + c.value(b, "BACKDROP") + ");");
    });
    r.statement("looks_nextbackdrop", [](const ir::Block&, TargetCompiler&, Emitter& out) {
        out.line("nextBackdrop();");
    });
    r.statement("looks_changesizeby", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("changeSizeBy(" + c.number(b, "CHANGE") + ");");
    });
    r.statement("looks_setsizeto", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("setSize(" + c.number(b, "SIZE") + ");");
    });
    r.statement("looks_changeeffectby", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        checkEffect(b, c);
        out.line("changeEffect(" + str::cppStringLiteral(str::toLower(c.field(b, "EFFECT", "color"))) + ", " + c.number(b, "CHANGE") + ");");
    });
    r.statement("looks_seteffectto", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        checkEffect(b, c);
        out.line("setEffect(" + str::cppStringLiteral(str::toLower(c.field(b, "EFFECT", "color"))) + ", " + c.number(b, "VALUE") + ");");
    });
    r.statement("looks_cleargraphiceffects", [](const ir::Block&, TargetCompiler&, Emitter& out) {
        out.line("clearEffects();");
    });
    r.statement("looks_show", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("show();");
    });
    r.statement("looks_hide", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (spriteOnly(b, c, out)) out.line("hide();");
    });
    r.statement("looks_gotofrontback", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (!spriteOnly(b, c, out)) return;
        out.line(c.field(b, "FRONT_BACK", "front") == "back" ? "goToBackLayer();" : "goToFrontLayer();");
    });
    r.statement("looks_goforwardbackwardlayers", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        if (!spriteOnly(b, c, out)) return;
        const std::string n = "static_cast<int>(" + c.number(b, "NUM") + ")";
        out.line((c.field(b, "FORWARD_BACKWARD", "forward") == "backward" ? "goBackwardLayers(" : "goForwardLayers(") + n + ");");
    });
    // Obsolete Scratch 2 blocks that still appear in some projects.
    r.statement("looks_hideallsprites", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        c.unsupportedStatement(b, out, "obsolete block");
    });

    r.expression("looks_costumenumbername", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::value(c.field(b, "NUMBER_NAME", "number") == "name" ? "costumeName()" : "costumeNumber()");
    });
    r.expression("looks_backdropnumbername", [](const ir::Block& b, TargetCompiler& c) {
        return Expr::value(c.field(b, "NUMBER_NAME", "number") == "name" ? "backdropName()" : "backdropNumber()");
    });
    r.expression("looks_size", [](const ir::Block&, TargetCompiler& c) {
        return c.isSprite() ? Expr::number("size()") : Expr::number("100.0");
    });
}

}  // namespace s2c::codegen
