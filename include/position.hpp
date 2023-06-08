#pragma once
#include <cmath>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <vector>
#include <utility>
#include <array>
#include <optional>

namespace XViewMap
{
struct Pos {
    double x = 0, y = 0, th = 0;
    Pos(double x, double y, double th = 0) : x(x), y(y), th(th) {}
    Pos() = default;
    // 相対座標を加算
    auto operator+(const Pos& rel) const
    {
        double c = cos(this->th), s = sin(this->th);
        return Pos{
            this->x + rel.x * c - rel.y * s, this->y + rel.y * c + rel.x * s, this->th + rel.th};
    }
    bool operator==(const Pos& rhs) const
    {
        return this->x == rhs.x && this->y == rhs.y && this->th == rhs.th;
    }
    bool operator!=(const Pos& rhs) const { return !(*this == rhs); }
};

class PositionHistory
{
private:
    std::mutex m;
    std::queue<Pos> to_update_queue;
    std::condition_variable update_cond;
    bool terminated = false;

public:
    std::vector<Pos> history;

    std::pair<std::optional<Pos>, Pos> waitPopLastNext()
    {
        std::unique_lock lock(m);
        while (to_update_queue.empty() && !terminated) {
            update_cond.wait(lock);
        }
        if (terminated) {
            return {};
        }
        Pos next = to_update_queue.front();
        to_update_queue.pop();
        std::optional<Pos> last = std::nullopt;
        if (!history.empty()) {
            last = history.back();
        }
        history.push_back(next);
        return std::make_pair(last, next);
    }
    std::optional<Pos> getNow()
    {
        std::lock_guard lock(m);
        if (to_update_queue.empty()) {
            if (history.empty()) {
                return std::nullopt;
            } else {
                return history.back();
            }
        } else {
            return to_update_queue.back();
        }
    }
    void push(Pos pos)
    {
        {
            std::lock_guard lock(m);
            bool diff = false;
            if (to_update_queue.empty()) {
                if (history.empty()) {
                    diff = true;
                } else {
                    diff = history.back() != pos;
                }
            } else {
                diff = to_update_queue.back() != pos;
            }
            if (diff) {
                to_update_queue.push(pos);
            }
        }
        update_cond.notify_one();
    }
    void reset(Pos pos)
    {
        {
            std::lock_guard lock(m);
            to_update_queue.push(pos);
            history.clear();
        }
        update_cond.notify_one();
    }

    void terminate()
    {
        terminated = true;
        update_cond.notify_all();
    }
};
}  // namespace XViewMap