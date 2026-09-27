#include "codegen/TargetCompiler.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>

#include "codegen/BlockRegistry.hpp"
#include "utils/Log.hpp"
#include "utils/StringUtils.hpp"

namespace s2c::codegen {

namespace {

std::string uniqueName(std::set<std::string>& used, const std::string& base) {
    if (used.insert(base).second) return base;
    for (int i = 2;; ++i) {
        const std::string candidate = base + "_" + std::to_string(i);
        if (used.insert(candidate).second) return candidate;
    }
}

const ir::Block* prototypeOf(const ir::Script& script) {
    if (!script.hat || script.hat->opcode != "procedures_definition") return nullptr;
    const ir::Input* in = script.hat->input("custom_block");
    if (!in || in->kind != ir::Input::Kind::Block || !in->block) return nullptr;
    return in->block.get();
}

}  // namespace

// ---- ProjectNames --------------------------------------------------------------

ProjectNames ProjectNames::build(const ir::Project& project) {
    ProjectNames names;
    std::set<std::string> usedClasses = {"ProjectStage"};
    for (const ir::Target& t : project.targets) {
        if (t.isStage) {
            names.classNames[&t] = "ProjectStage";
        } else {
            names.classNames[&t] = uniqueName(usedClasses, "Sprite_" + str::sanitizeIdentifier(t.name, "Sprite"));
        }
        std::set<std::string> used;
        names.variableConstants[&t];   // ensure entries exist even when empty
        names.listConstants[&t];
        for (const ir::Variable& v : t.variables) {
            names.variableConstants[&t][v.id] = uniqueName(used, "v_" + str::sanitizeIdentifier(v.name, "var"));
        }
        for (const ir::List& l : t.lists) {
            names.listConstants[&t][l.id] = uniqueName(used, "l_" + str::sanitizeIdentifier(l.name, "list"));
        }
    }
    return names;
}

// ---- TargetCompiler --------------------------------------------------------------

TargetCompiler::TargetCompiler(const ir::Project& project, const ir::Target& target, const ProjectNames& names,
                               Diagnostics& diagnostics)
    : project_(project), target_(target), names_(names), diagnostics_(diagnostics) {
    className_ = names_.classNames.at(&target_);
    baseClass_ = target_.isStage ? "scratch::Stage" : "scratch::Sprite";
    // Reserve names that already exist in the class scope.
    for (const auto& [id, name] : names_.variableConstants.at(&target_)) usedNames_.insert(name);
    for (const auto& [id, name] : names_.listConstants.at(&target_)) usedNames_.insert(name);
}

std::string TargetCompiler::freshName(const std::string& base) { return uniqueName(usedNames_, base); }

std::string TargetCompiler::literal(const ir::Literal& lit) const {
    switch (lit.kind) {
    case ir::Literal::Kind::Number: return "scratch::Value(" + str::cppDoubleLiteral(lit.number) + ")";
    case ir::Literal::Kind::Bool: return lit.boolean ? "scratch::Value(true)" : "scratch::Value(false)";
    case ir::Literal::Kind::String: return "scratch::Value(" + str::cppStringLiteral(lit.text) + ")";
    }
    return "scratch::Value()";
}

void TargetCompiler::collectProcedures() {
    for (const ir::Script& script : target_.scripts) {
        const ir::Block* proto = prototypeOf(script);
        if (!proto || !proto->mutation) continue;
        const ir::Mutation& m = *proto->mutation;
        if (procedures_.count(m.proccode)) {
            diagnostics_.warning("Duplicate custom block definition \"" + m.proccode + "\"; using the first one", target_.name);
            continue;
        }
        ProcedureInfo info;
        info.proccode = m.proccode;
        std::string base = str::replaceAll(str::replaceAll(m.proccode, "%s", ""), "%b", "");
        base = str::sanitizeIdentifier(str::trim(str::replaceAll(base, "%n", "")), "block");
        while (base.find("__") != std::string::npos) base = str::replaceAll(base, "__", "_");
        while (base.size() > 1 && base.back() == '_') base.pop_back();
        while (base.size() > 1 && base.front() == '_' && !std::isdigit(static_cast<unsigned char>(base[1]))) base.erase(0, 1);
        info.cppName = freshName("proc_" + base);
        info.argumentIds = m.argumentIds;
        info.argumentNames = m.argumentNames;
        std::set<std::string> usedParams = {"th"};
        for (const std::string& argName : m.argumentNames) {
            info.paramNames.push_back(uniqueName(usedParams, "arg_" + str::sanitizeIdentifier(argName, "param")));
        }
        info.warp = m.warp;
        info.definition = &script;
        procedures_[m.proccode] = std::move(info);
    }
}

const ProcedureInfo* TargetCompiler::procedure(const std::string& proccode) const {
    auto it = procedures_.find(proccode);
    return it == procedures_.end() ? nullptr : &it->second;
}

std::string TargetCompiler::argumentRef(const std::string& name) const {
    if (!currentProcedure_) return "";
    for (size_t i = 0; i < currentProcedure_->argumentNames.size(); ++i) {
        if (currentProcedure_->argumentNames[i] == name && i < currentProcedure_->paramNames.size()) {
            return currentProcedure_->paramNames[i];
        }
    }
    return "";
}

// ---- Inputs / expressions -------------------------------------------------------------

Expr TargetCompiler::expression(const ir::Input* input) {
    if (!input || input->kind == ir::Input::Kind::Empty) return Expr::value("scratch::Value()");
    switch (input->kind) {
    case ir::Input::Kind::Literal: {
        const ir::Literal& lit = input->literal;
        switch (lit.kind) {
        case ir::Literal::Kind::Number: return Expr::number(str::cppDoubleLiteral(lit.number));
        case ir::Literal::Kind::Bool: return Expr::boolean(lit.boolean ? "true" : "false");
        case ir::Literal::Kind::String: return Expr::string("std::string(" + str::cppStringLiteral(lit.text) + ")");
        }
        break;
    }
    case ir::Input::Kind::Block:
        if (input->block) return expression(*input->block);
        break;
    case ir::Input::Kind::Substack:
        diagnostics_.warning("A block stack was used where a value was expected", target_.name);
        break;
    case ir::Input::Kind::Empty:
        break;
    }
    return Expr::value("scratch::Value()");
}

Expr TargetCompiler::expression(const ir::Block& block) {
    // Drop-down menus are serialised as shadow blocks with a single field.
    if (block.inputs.empty() && block.fields.size() == 1 &&
        (block.shadow || str::endsWith(block.opcode, "_menu") || block.opcode == "math_number" || block.opcode == "text")) {
        const std::string& text = block.fields.begin()->second.value;
        if (str::startsWith(block.opcode, "math_")) {
            const std::string t = str::trim(text);
            char* end = nullptr;
            const double d = std::strtod(t.c_str(), &end);
            if (!t.empty() && end == t.c_str() + t.size() && std::isfinite(d)) {
                return Expr::number(str::cppDoubleLiteral(d));
            }
        }
        return Expr::string("std::string(" + str::cppStringLiteral(text) + ")");
    }
    if (const ExpressionHandler* handler = BlockRegistry::instance().findExpression(block.opcode)) {
        return (*handler)(block, *this);
    }
    return unsupportedExpression(block);
}

std::string TargetCompiler::number(const ir::Block& block, const std::string& input) {
    return expression(block.input(input)).asNumber();
}
std::string TargetCompiler::string(const ir::Block& block, const std::string& input) {
    return expression(block.input(input)).asString();
}
std::string TargetCompiler::boolean(const ir::Block& block, const std::string& input) {
    const ir::Input* in = block.input(input);
    if (!in || in->kind == ir::Input::Kind::Empty) return "false";   // empty boolean slot
    return expression(in).asBool();
}
std::string TargetCompiler::value(const ir::Block& block, const std::string& input) {
    return expression(block.input(input)).asValue();
}

std::string TargetCompiler::field(const ir::Block& block, const std::string& name, const std::string& fallback) const {
    return block.fieldValue(name, fallback);
}

std::string TargetCompiler::fieldLiteral(const ir::Block& block, const std::string& name, const std::string& fallback) const {
    return str::cppStringLiteral(block.fieldValue(name, fallback));
}

// ---- Statements -----------------------------------------------------------------------

void TargetCompiler::statement(const ir::Block& block, Emitter& out) {
    if (const StatementHandler* handler = BlockRegistry::instance().findStatement(block.opcode)) {
        (*handler)(block, *this, out);
        return;
    }
    unsupportedStatement(block, out);
}

void TargetCompiler::sequence(const ir::Sequence& seq, Emitter& out) {
    for (const ir::BlockPtr& b : seq.blocks) {
        if (b) statement(*b, out);
    }
}

void TargetCompiler::substack(const ir::Block& block, const std::string& input, Emitter& out) {
    const ir::Input* in = block.input(input);
    if (!in || in->kind != ir::Input::Kind::Substack) return;
    sequence(in->substack, out);
}

void TargetCompiler::unsupportedStatement(const ir::Block& block, Emitter& out, const std::string& detail) {
    if (reportedUnsupported_.insert(block.opcode).second) {
        diagnostics_.unsupportedBlock(block.opcode, target_.name, detail);
    }
    out.line("// Unsupported Scratch block: " + block.opcode + (detail.empty() ? "" : " (" + detail + ")"));
    out.line("unsupported(" + str::cppStringLiteral(block.opcode) + ");");
}

Expr TargetCompiler::unsupportedExpression(const ir::Block& block, const std::string& detail) {
    if (reportedUnsupported_.insert(block.opcode).second) {
        diagnostics_.unsupportedBlock(block.opcode, target_.name, detail);
    }
    return Expr::value("(unsupported(" + str::cppStringLiteral(block.opcode) + "), scratch::Value())");
}

// ---- Variables ----------------------------------------------------------------------------

std::string TargetCompiler::variableRef(const ir::Field& field) {
    const std::string id = field.id.value_or("");
    const ir::Target* stage = project_.stage();
    auto lookup = [&](const ir::Target& t) -> std::string {
        const auto& constants = names_.variableConstants.at(&t);
        const ir::Variable* var = t.variableById(id);
        if (!var) {
            for (const ir::Variable& v : t.variables) {
                if (v.name == field.value) { var = &v; break; }
            }
        }
        if (!var) return "";
        const std::string& constant = constants.at(var->id);
        if (&t == &target_) return "variables_[" + constant + "]";
        return "stage().variables_[" + names_.classNames.at(&t) + "::" + constant + "]";
    };
    std::string ref = lookup(target_);
    if (ref.empty() && stage && stage != &target_) ref = lookup(*stage);
    if (ref.empty()) {
        diagnostics_.warning("Variable \"" + field.value + "\" is not declared; it will be created at runtime", target_.name);
        ref = "dynamicVariable(" + str::cppStringLiteral(field.value) + ")";
    }
    return ref;
}

std::string TargetCompiler::listRef(const ir::Field& field) {
    const std::string id = field.id.value_or("");
    const ir::Target* stage = project_.stage();
    auto lookup = [&](const ir::Target& t) -> std::string {
        const auto& constants = names_.listConstants.at(&t);
        const ir::List* list = t.listById(id);
        if (!list) {
            for (const ir::List& l : t.lists) {
                if (l.name == field.value) { list = &l; break; }
            }
        }
        if (!list) return "";
        const std::string& constant = constants.at(list->id);
        if (&t == &target_) return "lists_[" + constant + "]";
        return "stage().lists_[" + names_.classNames.at(&t) + "::" + constant + "]";
    };
    std::string ref = lookup(target_);
    if (ref.empty() && stage && stage != &target_) ref = lookup(*stage);
    if (ref.empty()) {
        diagnostics_.warning("List \"" + field.value + "\" is not declared; it will be created at runtime", target_.name);
        ref = "dynamicList(" + str::cppStringLiteral(field.value) + ")";
    }
    return ref;
}

// ---- Hats ---------------------------------------------------------------------------------

bool TargetCompiler::hatInfo(const ir::Block& hat, HatInfo& info, size_t scriptIndex) {
    const std::string& op = hat.opcode;
    if (op == "event_whenflagclicked") { info.hatEnum = "GreenFlag"; return true; }
    if (op == "event_whenkeypressed") {
        info.hatEnum = "KeyPressed";
        info.option = str::toLower(field(hat, "KEY_OPTION", "space"));
        return true;
    }
    if (op == "event_whenthisspriteclicked") { info.hatEnum = "SpriteClicked"; return true; }
    if (op == "event_whenstageclicked") { info.hatEnum = "StageClicked"; return true; }
    if (op == "event_whenbroadcastreceived") {
        info.hatEnum = "BroadcastReceived";
        info.option = str::toLower(field(hat, "BROADCAST_OPTION"));
        return true;
    }
    if (op == "event_whenbackdropswitchesto") {
        info.hatEnum = "BackdropSwitched";
        info.option = field(hat, "BACKDROP");
        return true;
    }
    if (op == "control_start_as_clone") { info.hatEnum = "StartAsClone"; return true; }
    if (op == "event_whengreaterthan") {
        const std::string menu = str::toLower(field(hat, "WHENGREATERTHANMENU", "TIMER"));
        if (menu != "timer") {
            diagnostics_.unsupportedBlock(op, target_.name, "only the TIMER option is supported; script skipped");
            return false;
        }
        info.hatEnum = "TimerGreaterThan";
        info.predicateFn = "hatPredicate_" + std::to_string(scriptIndex);
        return true;
    }
    diagnostics_.unsupportedBlock(op, target_.name, "hat block; script skipped");
    return false;
}

// ---- Code emission --------------------------------------------------------------------------

std::string TargetCompiler::compileScriptBody(const ir::Script& script, const std::string& signature, bool warp) {
    Emitter out;
    out.open(signature);
    out.line("(void)th;");
    if (warp) out.line("scratch::WarpGuard warpGuard(th);   // \"run without screen refresh\"");
    sequence(script.body, out);
    out.line("co_return;");
    out.close();
    return out.str();
}

GeneratedTarget TargetCompiler::generate() {
    collectProcedures();

    std::vector<std::string> scriptDecls, procDecls, predicateDecls;
    std::vector<std::pair<HatInfo, std::string>> hats;   // hat info + method name
    Emitter bodies;

    // Custom blocks first so their names are known (they are, via collectProcedures).
    for (auto& [proccode, info] : procedures_) {
        std::string params = "scratch::Thread& th";
        for (const std::string& p : info.paramNames) params += ", scratch::Value " + p;
        procDecls.push_back("scratch::Task " + info.cppName + "(" + params + ");   // define " + proccode);
        currentProcedure_ = &info;
        bodies.raw("// Custom block: " + proccode + "\n");
        bodies.raw(compileScriptBody(*info.definition, "scratch::Task " + className_ + "::" + info.cppName + "(" + params + ")", info.warp));
        bodies.raw("\n");
        currentProcedure_ = nullptr;
    }

    size_t index = 0;
    for (const ir::Script& script : target_.scripts) {
        ++index;
        if (!script.hat) continue;
        if (script.hat->opcode == "procedures_definition") continue;
        HatInfo info;
        if (!hatInfo(*script.hat, info, index)) continue;
        const std::string method = "script_" + std::to_string(index);
        std::string comment = script.hat->opcode;
        if (!info.option.empty()) comment += " [" + info.option + "]";
        scriptDecls.push_back("scratch::Task " + method + "(scratch::Thread& th);   // " + comment);
        bodies.raw("// " + comment + "\n");
        bodies.raw(compileScriptBody(script, "scratch::Task " + className_ + "::" + method + "(scratch::Thread& th)", false));
        bodies.raw("\n");
        if (!info.predicateFn.empty()) {
            predicateDecls.push_back("bool " + info.predicateFn + "();");
            Emitter pred;
            pred.open("bool " + className_ + "::" + info.predicateFn + "()");
            pred.line("return timer() > " + number(*script.hat, "VALUE") + ";");
            pred.close();
            bodies.raw(pred.str());
            bodies.raw("\n");
        }
        hats.emplace_back(info, method);
    }

    GeneratedTarget result;
    result.className = className_;
    result.header = emitHeader(scriptDecls, procDecls, predicateDecls);

    Emitter src;
    src.line("// Generated by scratch2cpp from Scratch target \"" + target_.name + "\". Do not edit by hand.");
    src.line("#include \"targets/" + className_ + ".hpp\"");
    src.line();
    src.line("#include <cmath>");
    src.line("#include <limits>");
    src.line("#include <string>");
    src.line();
    if (!target_.isStage) src.line("#include \"targets/ProjectStage.hpp\"");
    src.line();
    src.raw(emitConstructor(hats));
    src.line();
    src.raw(bodies.str());
    result.source = src.str();
    return result;
}

std::string TargetCompiler::emitHeader(const std::vector<std::string>& scriptDecls, const std::vector<std::string>& procDecls,
                                       const std::vector<std::string>& predicateDecls) {
    Emitter h;
    h.line("// Generated by scratch2cpp from Scratch target \"" + target_.name + "\". Do not edit by hand.");
    h.line("#pragma once");
    h.line();
    h.line("#include <cstddef>");
    h.line("#include <memory>");
    h.line();
    h.line("#include \"scratch/Ops.hpp\"");
    h.line("#include \"scratch/Runtime.hpp\"");
    h.line(target_.isStage ? "#include \"scratch/Stage.hpp\"" : "#include \"scratch/Sprite.hpp\"");
    h.line();
    h.open("class " + className_ + " final : public " + baseClass_);
    h.dedent();
    h.line("public:");
    h.indent();
    h.line("explicit " + className_ + "(scratch::Runtime& runtime);");
    if (!target_.isStage) {
        h.open("std::unique_ptr<scratch::Sprite> cloneInstance() const override");
        h.line("return std::make_unique<" + className_ + ">(*this);");
        h.close();
    }
    const auto& vars = names_.variableConstants.at(&target_);
    if (!target_.variables.empty()) {
        h.line();
        h.line("// Variables (indices into variables_)");
        h.open("enum : std::size_t");
        for (size_t i = 0; i < target_.variables.size(); ++i) {
            const ir::Variable& v = target_.variables[i];
            h.line(vars.at(v.id) + " = " + std::to_string(i) + ",   // \"" + v.name + "\"");
        }
        h.close(";");
    }
    const auto& lists = names_.listConstants.at(&target_);
    if (!target_.lists.empty()) {
        h.line();
        h.line("// Lists (indices into lists_)");
        h.open("enum : std::size_t");
        for (size_t i = 0; i < target_.lists.size(); ++i) {
            const ir::List& l = target_.lists[i];
            h.line(lists.at(l.id) + " = " + std::to_string(i) + ",   // \"" + l.name + "\"");
        }
        h.close(";");
    }
    if (!scriptDecls.empty()) {
        h.line();
        h.line("// Scripts (one coroutine per hat block)");
        for (const std::string& d : scriptDecls) h.line(d);
    }
    if (!procDecls.empty()) {
        h.line();
        h.line("// Custom blocks");
        for (const std::string& d : procDecls) h.line(d);
    }
    if (!predicateDecls.empty()) {
        h.line();
        h.line("// Edge-triggered hat conditions");
        for (const std::string& d : predicateDecls) h.line(d);
    }
    h.close(";");
    return h.str();
}

std::string TargetCompiler::emitConstructor(const std::vector<std::pair<HatInfo, std::string>>& hats) {
    Emitter c;
    const std::string init = target_.isStage
        ? "scratch::Stage(runtime, " + std::to_string(project_.stageWidth) + ", " + std::to_string(project_.stageHeight) + ")"
        : "scratch::Sprite(runtime, " + str::cppStringLiteral(target_.name) + ")";
    c.open(className_ + "::" + className_ + "(scratch::Runtime& runtime) : " + init);

    c.line("// Costumes");
    for (const ir::Costume& costume : target_.costumes) {
        c.line("addCostume(scratch::CostumeInfo{" + str::cppStringLiteral(costume.name) + ", " +
               str::cppStringLiteral(costume.md5ext) + ", " + str::cppStringLiteral(costume.dataFormat) + ", " +
               str::cppDoubleLiteral(costume.rotationCenterX) + ", " + str::cppDoubleLiteral(costume.rotationCenterY) + ", " +
               str::cppDoubleLiteral(costume.bitmapResolution) + "});");
    }
    c.line("setCurrentCostume(" + std::to_string(target_.currentCostume) + ");");
    if (!target_.sounds.empty()) {
        c.line("// Sounds");
        for (const ir::Sound& sound : target_.sounds) {
            c.line("addSound(scratch::SoundInfo{" + str::cppStringLiteral(sound.name) + ", " +
                   str::cppStringLiteral(sound.md5ext) + ", " + str::cppStringLiteral(sound.dataFormat) + "});");
        }
    }
    c.line("setVolume(" + str::cppDoubleLiteral(target_.volume) + ");");
    if (!target_.variables.empty()) {
        c.line("// Variables (order matches the enum in the header)");
        for (const ir::Variable& v : target_.variables) {
            c.line("addVariable(" + str::cppStringLiteral(v.name) + ", " + literal(v.initial) + ");");
        }
    }
    if (!target_.lists.empty()) {
        c.line("// Lists");
        for (const ir::List& l : target_.lists) {
            c.line("addList(" + str::cppStringLiteral(l.name) + ", {");
            c.indent();
            std::string row;
            for (size_t i = 0; i < l.initial.size(); ++i) {
                row += literal(l.initial[i]) + ", ";
                if (row.size() > 100 || i + 1 == l.initial.size()) {
                    c.line(row);
                    row.clear();
                }
            }
            c.dedent();
            c.line("});");
        }
    }
    if (!target_.isStage) {
        c.line("// Initial sprite state");
        c.line("setPosition(" + str::cppDoubleLiteral(target_.x) + ", " + str::cppDoubleLiteral(target_.y) + ");");
        c.line("setDirectionRaw(" + str::cppDoubleLiteral(target_.direction) + ");");
        c.line("setSizeRaw(" + str::cppDoubleLiteral(target_.size) + ");");
        c.line(std::string("setVisible(") + (target_.visible ? "true" : "false") + ");");
        c.line(std::string("setDraggable(") + (target_.draggable ? "true" : "false") + ");");
        const char* style = "AllAround";
        if (target_.rotationStyle == ir::RotationStyle::LeftRight) style = "LeftRight";
        if (target_.rotationStyle == ir::RotationStyle::DontRotate) style = "DontRotate";
        c.line(std::string("setRotationStyle(scratch::RotationStyle::") + style + ");");
    }
    if (!hats.empty()) {
        c.line("// Scripts");
        for (const auto& [info, method] : hats) {
            const std::string factory = "[](scratch::Target& t, scratch::Thread& th) { return static_cast<" + className_ +
                                        "&>(t)." + method + "(th); }";
            if (!info.predicateFn.empty()) {
                c.line("addEdgeScript([](scratch::Target& t) { return static_cast<" + className_ + "&>(t)." + info.predicateFn +
                       "(); }, " + factory + ");");
            } else {
                c.line("addScript(scratch::Thread::Hat::" + info.hatEnum + ", " + str::cppStringLiteral(info.option) + ", " +
                       factory + ");");
            }
        }
    }
    c.close();
    return c.str();
}

}  // namespace s2c::codegen
