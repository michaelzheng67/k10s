#include "source/fake_source.hpp"

#include <chrono>
#include <thread>

namespace k10s {

namespace {
void pushAll(const std::vector<Event>& events, EventQueue& out, const std::stop_token& st) {
    for (const auto& e : events) {
        while (!out.try_push(e) && !st.stop_requested()) {
            std::this_thread::yield();
        }
    }
}
} // namespace

void FakeSource::run(std::stop_token st, EventQueue& out) {
    pushAll(sim_.snapshot(), out, st);

    while (!st.stop_requested()) {
        for (const auto& e : sim_.tick()) {
            while (!out.try_push(e) && !st.stop_requested()) {
                std::this_thread::yield();
            }
        }
        for (int i = 0; i < 10 && !st.stop_requested(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

} // namespace k10s