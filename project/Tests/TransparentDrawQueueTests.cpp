#include "Engine/3D/TransparentDrawQueue.h"
#include <cassert>
#include <limits>

int main()
{
    struct Object { bool fog; };
    Object nearOff { false }, farOn { true }, equalOff { false }, equalOn { true };
    TransparentDrawQueue<Object> queue;
    assert(queue.Empty());
    assert(queue.Add(&nearOff, 10));
    assert(queue.Add(&farOn, 100));
    assert(queue.Add(&equalOff, 50));
    assert(queue.Add(&equalOn, 50));
    assert(queue.Add(&nearOff, 10));
    assert(!queue.Add(nullptr, 20));
    assert(!queue.Add(&nearOff, std::numeric_limits<float>::quiet_NaN()));
    assert(!queue.Add(&nearOff, std::numeric_limits<float>::infinity()));
    const auto& draws = queue.Sorted();
    assert(draws.size() == 4);
    assert(draws[0].object == &farOn && draws[3].object == &nearOff);
    assert(draws[1].object == &equalOff && draws[2].object == &equalOn);
    queue.Remove(&farOn);
    assert(queue.Sorted().size() == 3);
    assert(queue.Sorted()[0].object == &equalOff);
    queue.Clear();
    assert(queue.Empty());
    assert(queue.Add(&nearOff, 2));
    assert(queue.Sorted().size() == 1);
}
