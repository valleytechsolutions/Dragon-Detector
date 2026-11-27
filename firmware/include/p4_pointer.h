#pragma once

#include <stdint.h>

namespace p4ui {

enum class Gesture { None, SwipeUp };
struct Point { int x, y; };
inline Point rotatePoint(Point at,int size,unsigned quarterTurns){
  switch(quarterTurns&3){
    case 1:return {size-1-at.y,at.x};
    case 2:return {size-1-at.x,size-1-at.y};
    case 3:return {at.y,size-1-at.x};
    default:return at;
  }
}
struct PointerFrame { bool pressed; Point at; };
struct PointerBatch {
  PointerFrame frames[2];
  unsigned count;
  Gesture gesture;
  void add(bool pressed, Point at) { frames[count++] = {pressed, at}; }
};

class EdgePointer {
 public:
  explicit EdgePointer(int diameter) : size_(diameter) {}

  PointerBatch sample(bool down, Point at, uint32_t now, bool edgesEnabled) {
    PointerBatch out = {};
    if (!down) {
      // The touch IC may return zero coordinates on release.
      if (state_ == State::Pending) {
        out.add(true, start_);
        out.add(false, last_);
      } else if (state_ == State::Through) {
        out.add(false, last_);
      } else if (state_ == State::Matched && edgesEnabled) {
        out.gesture = edge_;
      }
      state_ = State::Idle;
      return out;
    }
    last_ = at;
    if (state_ == State::Idle) {
      start_ = at;
      started_ = now;
      edge_ = edgesEnabled ? edgeAt(at) : Gesture::None;
      state_ = edge_ == Gesture::None ? State::Through : State::Pending;
    }
    if (!edgesEnabled && (state_ == State::Pending || state_ == State::Matched)) {
      state_ = State::Cancelled;
    }
    if (state_ == State::Through) {
      out.add(true, at);
    } else if (state_ == State::Pending) {
      const int dx = at.x - start_.x, dy = at.y - start_.y;
      const int inward = -dy, cross = absValue(dx);
      if (inward >= size_ / 10 && inward * 4 >= cross * 5) {
        state_ = State::Matched;
      } else if (inward < -size_ / 30 || (cross >= size_ / 18 && cross > inward)
                 || (now - started_ >= 550u && inward < size_ / 32)) {
        // Pass a scroll, drag or long press to its original widget.
        state_ = State::Through;
        out.add(true, start_);
        if (at.x != start_.x || at.y != start_.y) out.add(true, at);
      }
    }
    return out;
  }

 private:
  enum class State { Idle, Pending, Matched, Through, Cancelled };
  static int absValue(int v) { return v < 0 ? -v : v; }

  Gesture edgeAt(Point at) const {
    if (at.x < 0 || at.x >= size_ || at.y < 0 || at.y >= size_) return Gesture::None;
    if (at.y < size_ * 72 / 100 || at.x < size_ / 8 || at.x > size_ * 7 / 8) return Gesture::None;
    // Start within the lower segment of the glass, including near the curved bezel.
    const int x = 2 * at.x - (size_ - 1), y = 2 * at.y - (size_ - 1);
    const int r2 = x*x + y*y;
    return r2 <= (size_ + 1)*(size_ + 1) ? Gesture::SwipeUp : Gesture::None;
  }

  int size_;
  State state_ = State::Idle;
  Gesture edge_ = Gesture::None;
  Point start_ = {}, last_ = {};
  uint32_t started_ = 0;
};

// observe() records actual committed views. back() only peeks so a rejected
// request (for example a running node) never consumes its return destination.
template<class View, unsigned Capacity = 16>
class Trail {
  static_assert(Capacity >= 2, "Two views are required for Back");
 public:
  void observe(const View &view, bool returning = false) {
    if (count_ && values_[count_-1] == view) return;
    if (returning && count_ > 1 && values_[count_-2] == view) {
      --count_;
      return;
    }
    if (count_ == Capacity) {
      for (unsigned i=1; i<count_; ++i) values_[i-1] = values_[i];
      --count_;
    }
    values_[count_++] = view;
  }

  bool back(View &destination) const {
    if (count_ < 2) return false;
    destination = values_[count_-2];
    return true;
  }
 private:
  View values_[Capacity] = {};
  unsigned count_ = 0;
};
}  // namespace p4ui
