#include "codegen/BlockRegistry.hpp"

namespace s2c::codegen {

BlockRegistry& BlockRegistry::instance() {
    static BlockRegistry registry;
    return registry;
}

BlockRegistry::BlockRegistry() {
    registerMotionBlocks(*this);
    registerLooksBlocks(*this);
    registerSoundBlocks(*this);
    registerEventBlocks(*this);
    registerControlBlocks(*this);
    registerSensingBlocks(*this);
    registerOperatorBlocks(*this);
    registerDataBlocks(*this);
    registerProcedureBlocks(*this);
    registerPenBlocks(*this);
}

}  // namespace s2c::codegen
