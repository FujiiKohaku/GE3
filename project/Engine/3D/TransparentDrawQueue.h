#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// Frame-local requests. Equal depths retain submission order, including mixed fog settings.
template<class Object>
class TransparentDrawQueue {
public:
    struct Entry { Object* object; float depth; };
    bool Add(Object* object, float depth) {
        if (!object || !std::isfinite(depth)) return false;
        if (std::none_of(entries_.begin(), entries_.end(),
            [&](const Entry& entry) { return entry.object == object; })) entries_.push_back({ object, depth });
        return true;
    }
    void Remove(Object* object) {
        std::erase_if(entries_, [&](const Entry& entry) { return entry.object == object; });
    }
    void Clear() { entries_.clear(); }
    bool Empty() const { return entries_.empty(); }
    const std::vector<Entry>& Sorted() {
        std::stable_sort(entries_.begin(), entries_.end(),
            [](const Entry& left, const Entry& right) { return left.depth > right.depth; });
        return entries_;
    }
private:
    std::vector<Entry> entries_;
};
