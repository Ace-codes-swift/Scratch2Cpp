// TargetCompiler.hpp - generates the C++ class for one Scratch target.
//
// For a sprite named "Player" this produces
//   include/targets/Sprite_Player.hpp   class Sprite_Player : public scratch::Sprite
//   src/targets/Sprite_Player.cpp       constructor (costumes, variables, hats) + scripts
// Each script becomes a coroutine `scratch::Task script_N(scratch::Thread&)`;
// each custom block becomes `scratch::Task proc_Name(scratch::Thread&, Value...)`.
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "codegen/Emitter.hpp"
#include "codegen/Expr.hpp"
#include "ir/Diagnostics.hpp"
#include "ir/Project.hpp"

namespace s2c::codegen {

struct ProcedureInfo {
    std::string proccode;
    std::string cppName;                       // proc_jump
    std::vector<std::string> argumentIds;
    std::vector<std::string> argumentNames;    // Scratch names
    std::vector<std::string> paramNames;       // C++ parameter names
    bool warp = false;
    const ir::Script* definition = nullptr;
};

// Shared naming information about every target, needed when a sprite refers
// to stage (global) variables or to another sprite's class.
struct ProjectNames {
    std::map<const ir::Target*, std::string> classNames;
    // Variable/list ids -> C++ constant names, per target.
    std::map<const ir::Target*, std::map<std::string, std::string>> variableConstants;
    std::map<const ir::Target*, std::map<std::string, std::string>> listConstants;

    static ProjectNames build(const ir::Project& project);
};

struct GeneratedTarget {
    std::string className;
    std::string header;      // contents of include/targets/<class>.hpp
    std::string source;      // contents of src/targets/<class>.cpp
};

class TargetCompiler {
public:
    TargetCompiler(const ir::Project& project, const ir::Target& target, const ProjectNames& names,
                   Diagnostics& diagnostics);

    GeneratedTarget generate();

    // ---- Helpers for block handlers -------------------------------------
    const ir::Target& target() const { return target_; }
    const ir::Project& project() const { return project_; }
    bool isStage() const { return target_.isStage; }
    bool isSprite() const { return !target_.isStage; }
    const std::string& className() const { return className_; }
    Diagnostics& diagnostics() { return diagnostics_; }

    // Compile an input by name with the requested static type.
    std::string number(const ir::Block& block, const std::string& input);
    std::string string(const ir::Block& block, const std::string& input);
    std::string boolean(const ir::Block& block, const std::string& input);
    std::string value(const ir::Block& block, const std::string& input);
    Expr expression(const ir::Input* input);
    Expr expression(const ir::Block& block);
    // Field (drop-down) value as plain text / as C++ string literal.
    std::string field(const ir::Block& block, const std::string& name, const std::string& fallback = "") const;
    std::string fieldLiteral(const ir::Block& block, const std::string& name, const std::string& fallback = "") const;

    // Emit the statements of a C-mouth input ("SUBSTACK", "SUBSTACK2").
    void substack(const ir::Block& block, const std::string& input, Emitter& out);
    void sequence(const ir::Sequence& seq, Emitter& out);
    void statement(const ir::Block& block, Emitter& out);

    // Variable / list access expressions (resolves local vs. global).
    std::string variableRef(const ir::Field& field);
    std::string listRef(const ir::Field& field);

    // Custom block support.
    const ProcedureInfo* procedure(const std::string& proccode) const;
    const ProcedureInfo* currentProcedure() const { return currentProcedure_; }
    // C++ expression for a custom block argument by Scratch name ("" if unknown).
    std::string argumentRef(const std::string& name) const;

    // Records an unsupported block and emits a runtime notice.
    void unsupportedStatement(const ir::Block& block, Emitter& out, const std::string& detail = "");
    Expr unsupportedExpression(const ir::Block& block, const std::string& detail = "");

    std::string freshName(const std::string& base);

private:
    struct HatInfo {
        std::string hatEnum;      // scratch::Thread::Hat::GreenFlag
        std::string option;       // plain text
        std::string predicateFn;  // for edge-triggered hats
    };

    void collectProcedures();
    bool hatInfo(const ir::Block& hat, HatInfo& info, size_t scriptIndex);
    std::string compileScriptBody(const ir::Script& script, const std::string& signature, bool warp);
    std::string emitHeader(const std::vector<std::string>& scriptDecls, const std::vector<std::string>& procDecls,
                           const std::vector<std::string>& predicateDecls);
    std::string emitConstructor(const std::vector<std::pair<HatInfo, std::string>>& hats);
    std::string literal(const ir::Literal& lit) const;

    const ir::Project& project_;
    const ir::Target& target_;
    const ProjectNames& names_;
    Diagnostics& diagnostics_;
    std::string className_;
    std::string baseClass_;
    std::map<std::string, ProcedureInfo> procedures_;   // by proccode
    const ProcedureInfo* currentProcedure_ = nullptr;
    std::set<std::string> usedNames_;
    std::set<std::string> reportedUnsupported_;
};

}  // namespace s2c::codegen
