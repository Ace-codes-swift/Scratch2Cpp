#include "scratch/Sprite.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

#include "scratch/Runtime.hpp"

namespace scratch {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kFenceWidth = 15.0;   // scratch-render FENCE_WIDTH

double degToRad(double d) { return d * kPi / 180.0; }
double radToDeg(double r) { return r * 180.0 / kPi; }

// MathUtil.wrapClamp(direction, -180, 179) from scratch-vm.
double wrapDirection(double direction) {
    const double range = 360.0;
    return direction - std::floor((direction + 180.0) / range) * range;
}

std::mt19937& rng() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}

}  // namespace

Sprite::Sprite(Runtime& runtime, std::string name) : Target(runtime, std::move(name)) {}

void Sprite::requestRedrawIfVisible() const {
    if (visible_) runtime_->requestRedraw();
}

// ---- Geometry ------------------------------------------------------------------

double Sprite::renderAngle() const {
    return rotationStyle_ == RotationStyle::AllAround ? direction_ - 90.0 : 0.0;
}

bool Sprite::renderFlipped() const {
    return rotationStyle_ == RotationStyle::LeftRight && direction_ < 0;
}

Rect Sprite::bounds() const {
    const CostumeInfo* info = currentCostume();
    const Costume* c = info ? info->rendered.get() : nullptr;
    if (!c || !c->valid()) {
        return Rect{x_, x_, y_, y_};
    }
    // Transform the opaque bounding box of the costume through the sprite's
    // scale / flip / rotation to get axis-aligned stage bounds.
    const double k = (size_ / 100.0) / c->scale;
    const double theta = degToRad(renderAngle());
    const double cs = std::cos(theta), sn = std::sin(theta);
    const bool flip = renderFlipped();

    double left = std::numeric_limits<double>::infinity(), right = -left;
    double bottom = left, top = -left;
    const double xs[2] = {static_cast<double>(c->opaqueLeft), static_cast<double>(c->opaqueRight + 1)};
    const double ys[2] = {static_cast<double>(c->opaqueTop), static_cast<double>(c->opaqueBottom + 1)};
    for (double tx : xs) {
        for (double ty : ys) {
            double lx = (tx - c->centerX) * k;
            const double ly = (ty - c->centerY) * k;
            if (flip) lx = -lx;
            const double sx = lx * cs - ly * sn;   // screen space (y down)
            const double sy = lx * sn + ly * cs;
            const double px = x_ + sx;
            const double py = y_ - sy;
            left = std::min(left, px); right = std::max(right, px);
            bottom = std::min(bottom, py); top = std::max(top, py);
        }
    }
    return Rect{left, right, bottom, top};
}

bool Sprite::stageToTexture(double px, double py, int& tx, int& ty) const {
    const CostumeInfo* info = currentCostume();
    const Costume* c = info ? info->rendered.get() : nullptr;
    if (!c || !c->valid()) return false;
    const double k = (size_ / 100.0) / c->scale;
    if (k <= 0) return false;
    const double theta = degToRad(renderAngle());
    const double cs = std::cos(theta), sn = std::sin(theta);
    const double dx = px - x_;
    const double dy = y_ - py;   // to screen orientation
    double lx = dx * cs + dy * sn;
    const double ly = -dx * sn + dy * cs;
    if (renderFlipped()) lx = -lx;
    tx = static_cast<int>(std::floor(c->centerX + lx / k));
    ty = static_cast<int>(std::floor(c->centerY + ly / k));
    return true;
}

bool Sprite::containsPoint(double px, double py) const {
    int tx = 0, ty = 0;
    if (!stageToTexture(px, py, tx, ty)) return false;
    return currentCostume()->rendered->alphaAt(tx, ty) > 0;
}

bool Sprite::colorAt(double px, double py, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const {
    int tx = 0, ty = 0;
    if (!stageToTexture(px, py, tx, ty)) return false;
    return currentCostume()->rendered->colorAt(tx, ty, r, g, b);
}

template <class Fn>
bool Sprite::forEachOpaquePoint(Fn fn) const {
    const Rect b = bounds();
    const double halfW = runtime_->stage().width() / 2.0;
    const double halfH = runtime_->stage().height() / 2.0;
    const double left = std::max(std::floor(b.left), -halfW);
    const double right = std::min(std::ceil(b.right), halfW);
    const double bottom = std::max(std::floor(b.bottom), -halfH);
    const double top = std::min(std::ceil(b.top), halfH);
    if (right < left || top < bottom) return false;
    // Sample at stage resolution, coarser for very large sprites.
    const double area = (right - left + 1) * (top - bottom + 1);
    const double step = std::max(1.0, std::floor(std::sqrt(area / 40000.0)));
    for (double py = bottom; py <= top; py += step) {
        for (double px = left; px <= right; px += step) {
            if (containsPoint(px + 0.5, py + 0.5) && fn(px + 0.5, py + 0.5)) return true;
        }
    }
    return false;
}

bool Sprite::touchingSprite(const Sprite& other) const {
    if (!visible_ || !other.visible_ || other.deleted_) return false;
    const Rect a = bounds();
    const Rect b = other.bounds();
    if (!a.intersects(b)) return false;
    const double left = std::floor(std::max(a.left, b.left));
    const double right = std::ceil(std::min(a.right, b.right));
    const double bottom = std::floor(std::max(a.bottom, b.bottom));
    const double top = std::ceil(std::min(a.top, b.top));
    // Sample the overlap at stage resolution and look for a pixel where both
    // costumes are opaque (Scratch does the same on the GPU).
    for (double py = bottom; py <= top; py += 1.0) {
        for (double px = left; px <= right; px += 1.0) {
            if (containsPoint(px + 0.5, py + 0.5) && other.containsPoint(px + 0.5, py + 0.5)) return true;
        }
    }
    return false;
}

// ---- Motion --------------------------------------------------------------------

void Sprite::keepInFence() {
    // scratch-render getFencedPositionOfDrawable: keep at least a sliver of
    // the sprite visible so it can never disappear off-stage.
    const Rect aabb = bounds();
    if (!std::isfinite(aabb.width()) || aabb.width() <= 0 || aabb.height() <= 0) return;
    const double halfW = runtime_->stage().width() / 2.0;
    const double halfH = runtime_->stage().height() / 2.0;
    const double inset = std::floor(std::min(aabb.width(), aabb.height()) / 2.0);
    const double sx = halfW - std::min(kFenceWidth, inset);
    if (aabb.right < -sx) {
        x_ = std::ceil(x_ - (sx + aabb.right));
    } else if (aabb.left > sx) {
        x_ = std::floor(x_ + (sx - aabb.left));
    }
    const double sy = halfH - std::min(kFenceWidth, inset);
    if (aabb.top < -sy) {
        y_ = std::ceil(y_ - (sy + aabb.top));
    } else if (aabb.bottom > sy) {
        y_ = std::floor(y_ + (sy - aabb.bottom));
    }
}

void Sprite::goToXY(double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    const double ox = x_;
    const double oy = y_;
    x_ = x;
    y_ = y;
    keepInFence();
    if (pen_.down) {
        runtime_->penLayer().drawLine(ox, oy, x_, y_, pen_);
        runtime_->requestRedraw();
    } else {
        requestRedrawIfVisible();
    }
}

void Sprite::stampCostume() {
    const double ghost = std::clamp(effect("ghost"), 0.0, 100.0);
    const std::uint8_t a = static_cast<std::uint8_t>(std::lround((1.0 - ghost / 100.0) * 255.0));
    if (a == 0) return;
    forEachOpaquePoint([&](double px, double py) {
        std::uint8_t r = 0, g = 0, b = 0;
        if (colorAt(px, py, r, g, b)) runtime_->penLayer().blendStage(px, py, r, g, b, a);
        return false;
    });
    runtime_->requestRedraw();
}

void Sprite::moveSteps(double steps) {
    const double radians = degToRad(90.0 - direction_);
    goToXY(x_ + steps * std::cos(radians), y_ + steps * std::sin(radians));
}

void Sprite::pointInDirection(double direction) {
    if (!std::isfinite(direction)) return;
    direction_ = wrapDirection(direction);
    requestRedrawIfVisible();
}

void Sprite::turnRight(double degrees) { pointInDirection(direction_ + degrees); }
void Sprite::turnLeft(double degrees) { pointInDirection(direction_ - degrees); }

bool Sprite::resolveTargetPoint(const Value& target, double& outX, double& outY) const {
    const std::string name = target.toString();
    if (name == "_mouse_") {
        outX = runtime_->input().mouseX();
        outY = runtime_->input().mouseY();
        return true;
    }
    if (name == "_random_") {
        const int w = runtime_->stage().width();
        const int h = runtime_->stage().height();
        outX = std::uniform_int_distribution<int>(-w / 2, w / 2)(rng());
        outY = std::uniform_int_distribution<int>(-h / 2, h / 2)(rng());
        return true;
    }
    if (const Sprite* s = runtime_->findSprite(name)) {
        outX = s->x();
        outY = s->y();
        return true;
    }
    return false;
}

void Sprite::goTo(const Value& target) {
    double tx = 0, ty = 0;
    if (resolveTargetPoint(target, tx, ty)) goToXY(tx, ty);
}

Task Sprite::glideTo(double seconds, double x, double y) {
    if (seconds <= 0 || !std::isfinite(seconds)) {
        goToXY(x, y);
        co_return;
    }
    const double startX = x_, startY = y_;
    const double start = Runtime::now();
    runtime_->requestRedraw();
    co_await Yield{};
    for (;;) {
        const double frac = (Runtime::now() - start) / seconds;
        if (frac >= 1.0) {
            goToXY(x, y);
            break;
        }
        goToXY(startX + frac * (x - startX), startY + frac * (y - startY));
        runtime_->requestRedraw();
        co_await Yield{};
    }
}

Task Sprite::glideToTarget(double seconds, const Value& target) {
    double tx = 0, ty = 0;
    if (!resolveTargetPoint(target, tx, ty)) co_return;
    co_await glideTo(seconds, tx, ty);
}

void Sprite::pointTowards(const Value& target) {
    const std::string name = target.toString();
    if (name == "_random_") {
        pointInDirection(std::uniform_int_distribution<int>(-180, 179)(rng()));
        return;
    }
    double tx = 0, ty = 0;
    if (!resolveTargetPoint(target, tx, ty)) return;
    const double dx = tx - x_;
    const double dy = ty - y_;
    pointInDirection(90.0 - radToDeg(std::atan2(dy, dx)));
}

void Sprite::ifOnEdgeBounce() {
    const Rect b = bounds();
    const double halfW = runtime_->stage().width() / 2.0;
    const double halfH = runtime_->stage().height() / 2.0;
    const double distLeft = std::max(0.0, halfW + b.left);
    const double distTop = std::max(0.0, halfH - b.top);
    const double distRight = std::max(0.0, halfW - b.right);
    const double distBottom = std::max(0.0, halfH + b.bottom);
    enum Edge { None, Left, Top, Right, Bottom } nearest = None;
    double minDist = std::numeric_limits<double>::infinity();
    if (distLeft < minDist) { minDist = distLeft; nearest = Left; }
    if (distTop < minDist) { minDist = distTop; nearest = Top; }
    if (distRight < minDist) { minDist = distRight; nearest = Right; }
    if (distBottom < minDist) { minDist = distBottom; nearest = Bottom; }
    if (minDist > 0) return;   // not touching any edge

    const double radians = degToRad(90.0 - direction_);
    double dx = std::cos(radians);
    double dy = -std::sin(radians);
    switch (nearest) {
    case Left: dx = std::max(0.2, std::fabs(dx)); break;
    case Top: dy = std::max(0.2, std::fabs(dy)); break;
    case Right: dx = -std::max(0.2, std::fabs(dx)); break;
    case Bottom: dy = -std::max(0.2, std::fabs(dy)); break;
    case None: break;
    }
    pointInDirection(radToDeg(std::atan2(dy, dx)) + 90.0);

    // Keep the sprite fully inside the stage (RenderedTarget.keepInFence).
    const Rect nb = bounds();
    double fx = 0, fy = 0;
    if (nb.left < -halfW) fx += -halfW - nb.left;
    if (nb.right > halfW) fx += halfW - nb.right;
    if (nb.top > halfH) fy += halfH - nb.top;
    if (nb.bottom < -halfH) fy += -halfH - nb.bottom;
    goToXY(x_ + fx, y_ + fy);
}

void Sprite::setRotationStyle(const Value& style) {
    const std::string s = style.toString();
    if (s == "left-right") rotationStyle_ = RotationStyle::LeftRight;
    else if (s == "don't rotate") rotationStyle_ = RotationStyle::DontRotate;
    else rotationStyle_ = RotationStyle::AllAround;
    requestRedrawIfVisible();
}

// ---- Looks --------------------------------------------------------------------

void Sprite::setBubble(const Value& text, bool think) {
    std::string message;
    if (text.isNumber()) {
        // Scratch rounds non-integer numbers to 2 decimals in bubbles.
        const double n = text.toNumber();
        message = Value(std::trunc(n) == n ? n : std::round(n * 100.0) / 100.0).toString();
    } else {
        message = text.toString();
    }
    bubble_.text = message;
    bubble_.think = think;
    bubble_.visible = !message.empty();
    bubble_.token = ++bubbleCounter_;
    runtime_->requestRedraw();
}

void Sprite::say(const Value& text) { setBubble(text, false); }
void Sprite::think(const Value& text) { setBubble(text, true); }

Task Sprite::sayForSecs(const Value& text, double seconds) {
    setBubble(text, false);
    const int token = bubble_.token;
    co_await wait(seconds);
    if (bubble_.token == token) {
        bubble_.visible = false;
        bubble_.text.clear();
        runtime_->requestRedraw();
    }
}

Task Sprite::thinkForSecs(const Value& text, double seconds) {
    setBubble(text, true);
    const int token = bubble_.token;
    co_await wait(seconds);
    if (bubble_.token == token) {
        bubble_.visible = false;
        bubble_.text.clear();
        runtime_->requestRedraw();
    }
}

void Sprite::switchCostumeTo(const Value& selector) {
    if (std::optional<int> idx = resolveCostume(selector, /*isBackdrop=*/false)) {
        setCostumeByIndex(*idx);
    }
}

void Sprite::nextCostume() { setCostumeByIndex(costume_ + 1); }

void Sprite::setSize(double size) {
    if (!std::isfinite(size)) return;
    const CostumeInfo* info = currentCostume();
    const Costume* c = info ? info->rendered.get() : nullptr;
    if (c && c->valid()) {
        // RenderedTarget.setSize clamps so the costume stays between 5 px and
        // 1.5x the stage in size.
        const double origW = c->width / c->scale;
        const double origH = c->height / c->scale;
        const double minScale = std::min(1.0, std::max(5.0 / origW, 5.0 / origH));
        const double maxScale = std::min((1.5 * runtime_->stage().width()) / origW,
                                         (1.5 * runtime_->stage().height()) / origH);
        size_ = std::clamp(size / 100.0, minScale, maxScale) * 100.0;
    } else {
        size_ = std::max(0.0, size);
    }
    requestRedrawIfVisible();
}

void Sprite::show() {
    visible_ = true;
    runtime_->requestRedraw();
}

void Sprite::hide() {
    visible_ = false;
    runtime_->requestRedraw();
}

void Sprite::goToFrontLayer() { runtime_->moveToFront(*this); }
void Sprite::goToBackLayer() { runtime_->moveToBack(*this); }
void Sprite::goForwardLayers(int n) { runtime_->moveLayers(*this, n); }
void Sprite::goBackwardLayers(int n) { runtime_->moveLayers(*this, -n); }

// ---- Control ------------------------------------------------------------------

void Sprite::deleteThisClone() {
    if (!isClone_ || deleted_) return;
    runtime_->deleteClone(*this);
}

// ---- Sensing -------------------------------------------------------------------

bool Sprite::touching(const Value& target) const {
    if (!visible_) return false;
    const std::string name = target.toString();
    if (name == "_mouse_") {
        const Input& in = runtime_->input();
        return containsPoint(in.mouseX(), in.mouseY());
    }
    if (name == "_edge_") {
        const Rect b = bounds();
        const double halfW = runtime_->stage().width() / 2.0;
        const double halfH = runtime_->stage().height() / 2.0;
        return b.left < -halfW || b.right > halfW || b.top > halfH || b.bottom < -halfH;
    }
    for (const Sprite* other : runtime_->spritesNamed(name)) {
        if (other != this && touchingSprite(*other)) return true;
    }
    return false;
}

bool Sprite::touchingColor(const Value& color) const {
    if (!visible_) return false;
    std::uint8_t tr, tg, tb;
    parseScratchColor(color, tr, tg, tb);
    return forEachOpaquePoint([&](double px, double py) {
        std::uint8_t r, g, b;
        runtime_->sceneColorAt(px, py, this, r, g, b);
        return colorMatches(r, g, b, tr, tg, tb);
    });
}

bool Sprite::colorIsTouchingColor(const Value& own, const Value& other) const {
    if (!visible_) return false;
    std::uint8_t or_, og, ob, tr, tg, tb;
    parseScratchColor(own, or_, og, ob);
    parseScratchColor(other, tr, tg, tb);
    return forEachOpaquePoint([&](double px, double py) {
        std::uint8_t r, g, b;
        if (!colorAt(px, py, r, g, b) || !colorMatches(r, g, b, or_, og, ob)) return false;
        runtime_->sceneColorAt(px, py, this, r, g, b);
        return colorMatches(r, g, b, tr, tg, tb);
    });
}

double Sprite::distanceTo(const Value& target) const {
    double tx = 0, ty = 0;
    const std::string name = target.toString();
    if (name == "_random_") return 10000.0;
    if (!resolveTargetPoint(target, tx, ty)) return 10000.0;
    return std::sqrt((tx - x_) * (tx - x_) + (ty - y_) * (ty - y_));
}

}  // namespace scratch
