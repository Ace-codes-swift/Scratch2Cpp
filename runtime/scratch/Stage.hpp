// Stage.hpp - the Scratch stage (backdrops + global variables/scripts).
#pragma once

#include <string>

#include "scratch/Target.hpp"

namespace scratch {

class Stage : public Target {
public:
    Stage(Runtime& runtime, int width = 480, int height = 360);

    bool isStage() const override { return true; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Switching a backdrop fires "when backdrop switches to" hats.
    void setCostumeByIndex(int index) override;

private:
    int width_;
    int height_;
};

}  // namespace scratch
