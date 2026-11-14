#pragma once

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace k10s {

// k9s-style keyboard UI:
//   :            open the command bar
//   :ctx / :contexts      open the context picker (type to filter, up/down, Enter)
//   :ctx <name>  switch directly
//   :q / :quit   exit
//   Esc          close whatever is open
class CommandUi {
  public:
    enum class ActionKind { None, SwitchContext, Quit };
    struct Action {
        ActionKind kind{ActionKind::None};
        std::string arg;
    };

    // Reads this frame's keyboard input. Call once per frame, before drawing.
    Action update(const std::vector<std::string>& contexts, const std::string& current, float dt);

    void draw(const std::vector<std::string>& contexts, const std::string& current, int screenW,
              int screenH) const;

    // Shows a short message in the command bar area (e.g. "unknown context").
    void flash(std::string msg);

  private:
    enum class Mode { Idle, Command, Picker };

    Action runCommand(const std::vector<std::string>& contexts);
    std::vector<std::size_t> filtered(const std::vector<std::string>& contexts) const;

    Mode mode_{Mode::Idle};
    std::string text_;        // command text, or picker filter
    std::size_t selected_{0}; // index into filtered list
    bool justOpened_{false};
    std::string message_;
    float messageTimer_{0.0f};
};

} // namespace k10s
