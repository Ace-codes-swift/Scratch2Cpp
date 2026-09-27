// SoundBlocks.cpp - "Sound" palette.
#include "codegen/BlockRegistry.hpp"
#include "codegen/Emitter.hpp"
#include "codegen/TargetCompiler.hpp"

namespace s2c::codegen {

void registerSoundBlocks(BlockRegistry& r) {
    r.statement("sound_playuntildone", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("co_await playSoundUntilDone(" + c.value(b, "SOUND_MENU") + ");");
    });
    r.statement("sound_play", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("startSound(" + c.value(b, "SOUND_MENU") + ");");
    });
    r.statement("sound_stopallsounds", [](const ir::Block&, TargetCompiler&, Emitter& out) {
        out.line("stopAllSounds();");
    });
    r.statement("sound_changevolumeby", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("changeVolumeBy(" + c.number(b, "VOLUME") + ");");
    });
    r.statement("sound_setvolumeto", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        out.line("setVolumeTo(" + c.number(b, "VOLUME") + ");");
    });
    // Pitch / pan effects need a DSP stage we do not have.
    r.statement("sound_changeeffectby", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        c.unsupportedStatement(b, out, "sound effects (pitch/pan) are not implemented");
    });
    r.statement("sound_seteffectto", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
        c.unsupportedStatement(b, out, "sound effects (pitch/pan) are not implemented");
    });
    r.statement("sound_cleareffects", [](const ir::Block&, TargetCompiler&, Emitter& out) {
        out.line("// sound_cleareffects: sound effects are not implemented, nothing to clear");
    });

    r.expression("sound_volume", [](const ir::Block&, TargetCompiler&) { return Expr::number("volume()"); });
}

}  // namespace s2c::codegen
