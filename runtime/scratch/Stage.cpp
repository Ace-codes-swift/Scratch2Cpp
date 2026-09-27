#include "scratch/Stage.hpp"

#include "scratch/Runtime.hpp"

namespace scratch {

Stage::Stage(Runtime& runtime, int width, int height)
    : Target(runtime, "Stage"), width_(width), height_(height) {}

void Stage::setCostumeByIndex(int index) {
    Target::setCostumeByIndex(index);
    if (const CostumeInfo* c = currentCostume()) {
        runtime_->startHats(Thread::Hat::BackdropSwitched, c->name);
    }
}

}  // namespace scratch
