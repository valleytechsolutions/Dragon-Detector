#pragma once
#include <stdint.h>
#include "waveshare_185b_touch.h"

namespace waveshare185b {

enum class PointerKind : uint8_t { Idle, Down, Move, Hold, Up, Cancel };
struct PointerReport {
  PointerKind kind;
  uint16_t x, y;
  uint32_t at;
  bool pressed() const {
    return kind == PointerKind::Down || kind == PointerKind::Move || kind == PointerKind::Hold;
  }
};

// The sensor task publishes into this bounded buffer. The UI task reads it
// under the same lock, then calls LVGL after releasing the lock. Keep turns in
// a drag path so even a swipe completed between frames reaches LVGL as motion.
class PointerBuffer {
 public:
  struct Stats {
    uint32_t coalesced = 0, overflow = 0, expired = 0, faults = 0;
    uint32_t cancels = 0, lastLagMs = 0, maxLagMs = 0;
    unsigned peakDepth = 0;
  } stats;

  bool hasWork() const { return cancelPending || count != 0; }

  void resetView() {
    if (sourceDown || deliveredDown || hasWork()) {
      cancelPending = true;
      waitForLift = waitForLift || sourceDown;
      count = 0;
    }
  }

  void sample(bool down, uint16_t x, uint16_t y, bool valid, uint32_t now) {
    if (!valid) {
      // Short read failures retain a held contact in TouchState. A prolonged
      // failure cancels it, without delivering an ordinary LVGL click.
      if (sourceDown && !down) {
        ++stats.faults;
        cancelPending = true;
        waitForLift = true;
        sourceDown = false;
        count = 0;
      }
      return;
    }
    const bool wasDown = sourceDown;
    sourceDown = down;
    lastGood = now;
    if (waitForLift) {
      if (!down) waitForLift = false;
      return;
    }
    if (down && !wasDown) {
      append({PointerKind::Down, x, y, now});
    } else if (!down && wasDown) {
      // The controller may clear its coordinates on lift. Keep the last
      // pressed point, including any movement queued before this transition.
      append({PointerKind::Up, sourceX, sourceY, now});
    } else if (down && (x != sourceX || y != sourceY)) {
      pushMove(x, y, now);
    }
    if (down) { sourceX = x; sourceY = y; }
  }

  PointerReport read(uint32_t now) {
    const bool abandonedHold = deliveredDown &&
        (!inputFresh(lastRead, now) || !inputFresh(lastGood, now));
    lastRead = now;
    if (cancelPending) {
      cancelPending = false;
      return cancel(now);
    }
    if (abandonedHold) {
      ++stats.expired;
      discardUntilDown();
      return cancel(now);
    }
    while (count) {
      PointerReport event = pop();
      if (!inputFresh(event.at, now)) {
        ++stats.expired;
        discardUntilDown();
        return cancel(now);
      }
      // Never resume the tail of a gesture whose press was canceled.
      if (event.kind != PointerKind::Down && !deliveredDown) continue;
      deliveredDown = event.kind != PointerKind::Up;
      deliveredX = event.x;
      deliveredY = event.y;
      stats.lastLagMs = now - event.at;
      if (stats.maxLagMs < stats.lastLagMs) stats.maxLagMs = stats.lastLagMs;
      return event;
    }
    return {deliveredDown ? PointerKind::Hold : PointerKind::Idle, deliveredX, deliveredY, now};
  }

 private:
  static constexpr unsigned Capacity = 32;
  PointerReport events[Capacity]{};
  unsigned head = 0, count = 0;
  uint16_t sourceX = 0, sourceY = 0, segmentX = 0, segmentY = 0;
  uint16_t deliveredX = 0, deliveredY = 0;
  uint32_t lastRead = 0, lastGood = 0;
  bool sourceDown = false, deliveredDown = false, cancelPending = false, waitForLift = false;

  void append(PointerReport event) {
    if (count == Capacity) {
      ++stats.overflow;
      count = 0;
      cancelPending = true;
      waitForLift = sourceDown;
      return;
    }
    events[(head + count) % Capacity] = event;
    if (++count > stats.peakDepth) stats.peakDepth = count;
  }
  void pushMove(uint16_t x, uint16_t y, uint32_t at) {
    if (count) {
      PointerReport& tail = events[(head + count - 1) % Capacity];
      if (tail.kind == PointerKind::Move) {
        int dx = int(tail.x) - segmentX, dy = int(tail.y) - segmentY;
        int nextDx = int(x) - tail.x, nextDy = int(y) - tail.y;
        // Combine roughly straight motion. Retain reversals and corners so
        // the drag still crosses the scroll threshold even when it returns.
        int dot = dx * nextDx + dy * nextDy;
        int cross = dx * nextDy - dy * nextDx;
        if (dot > 0 && 2 * (cross < 0 ? -cross : cross) <= dot) {
          tail = {PointerKind::Move, x, y, at};
          ++stats.coalesced;
          return;
        }
      }
    }
    segmentX = sourceX;
    segmentY = sourceY;
    append({PointerKind::Move, x, y, at});
  }
  PointerReport pop() {
    PointerReport event = events[head];
    head = (head + 1) % Capacity;
    --count;
    return event;
  }
  void discardUntilDown() {
    while (count && events[head].kind != PointerKind::Down) pop();
  }
  PointerReport cancel(uint32_t now) {
    deliveredDown = false;
    ++stats.cancels;
    return {PointerKind::Cancel, deliveredX, deliveredY, now};
  }
};
}  // namespace waveshare185b
