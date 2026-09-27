// BlockRegistry.hpp - opcode -> C++ translation handlers.
//
// Each Scratch block category (motion, looks, control, ...) lives in its own
// file under codegen/blocks/ and registers handlers here. Adding support for
// a new block means adding one handler; nothing else needs to change.
#pragma once

#include <functional>
#include <map>
#include <string>

#include "codegen/Expr.hpp"
#include "ir/Project.hpp"

namespace s2c::codegen {

class Emitter;
class TargetCompiler;

// Emits statements for a stack block.
using StatementHandler = std::function<void(const ir::Block&, TargetCompiler&, Emitter&)>;
// Produces an expression for a reporter/boolean block.
using ExpressionHandler = std::function<Expr(const ir::Block&, TargetCompiler&)>;

class BlockRegistry {
public:
    static BlockRegistry& instance();

    void statement(const std::string& opcode, StatementHandler handler) { statements_[opcode] = std::move(handler); }
    void expression(const std::string& opcode, ExpressionHandler handler) { expressions_[opcode] = std::move(handler); }

    const StatementHandler* findStatement(const std::string& opcode) const {
        auto it = statements_.find(opcode);
        return it == statements_.end() ? nullptr : &it->second;
    }
    const ExpressionHandler* findExpression(const std::string& opcode) const {
        auto it = expressions_.find(opcode);
        return it == expressions_.end() ? nullptr : &it->second;
    }
    const std::map<std::string, StatementHandler>& statements() const { return statements_; }
    const std::map<std::string, ExpressionHandler>& expressions() const { return expressions_; }

private:
    BlockRegistry();
    std::map<std::string, StatementHandler> statements_;
    std::map<std::string, ExpressionHandler> expressions_;
};

// Implemented in codegen/blocks/*.cpp.
void registerMotionBlocks(BlockRegistry&);
void registerLooksBlocks(BlockRegistry&);
void registerSoundBlocks(BlockRegistry&);
void registerEventBlocks(BlockRegistry&);
void registerControlBlocks(BlockRegistry&);
void registerSensingBlocks(BlockRegistry&);
void registerOperatorBlocks(BlockRegistry&);
void registerDataBlocks(BlockRegistry&);
void registerProcedureBlocks(BlockRegistry&);
void registerPenBlocks(BlockRegistry&);

}  // namespace s2c::codegen
