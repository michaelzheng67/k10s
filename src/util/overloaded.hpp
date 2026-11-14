#pragma once

namespace k10s {

// Lets you pass several lambdas to std::visit, one per variant alternative.
template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};

} // namespace k10s
