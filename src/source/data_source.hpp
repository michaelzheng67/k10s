#pragma once

#include <stop_token>

#include "model/events.hpp"
#include "util/spsc_queue.hpp"

namespace k10s {
using EventQueue = SpscQueue<Event, 4096>;

class DataSource {
  public:
    virtual ~DataSource() = default;
    virtual void run(std::stop_token st, EventQueue& out) = 0;
};
} // namespace k10s