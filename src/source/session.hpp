#pragma once

#include <memory>
#include <thread>
#include <utility>

#include "source/data_source.hpp"

namespace k10s {

class Session {
  public:
    explicit Session(std::unique_ptr<DataSource> source)
        : source_(std::move(source)),
          thread_([this](std::stop_token st) { source_->run(st, queue_); }) {}

    EventQueue& events() { return queue_; }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

  private:
    std::unique_ptr<DataSource> source_;
    EventQueue queue_;
    std::jthread thread_;
};
} // namespace k10s