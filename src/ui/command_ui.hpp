#pragma once

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace k10s {

// k9s-style keyboard UI:
//   :                 open the command bar
//   :ctx              open the context picker (type to filter, up/down, Enter)
//   :ctx <name>       switch context directly
//   :theme            open the theme picker      :theme <name>  switch directly
//   :q / :quit        exit
//   Esc               close whatever is open
class CommandUi {
  public:
    enum class ActionKind { None, SwitchContext, SwitchTheme, Quit };
    struct Action {
        ActionKind kind{ActionKind::None};
        std::string arg;
    };

    // What the pickers choose from.
    struct Choices {
        const std::vector<std::string>& contexts;
        const std::string& context; // current
        const std::vector<std::string>& themes;
        const std::string& theme; // current
    };

    // Reads this frame's keyboard input. Call once per frame, before drawing.
    Action update(const Choices& choices, float dt);
    void draw(const Choices& choices, int screenW, int screenH) const;

    // True while the command bar or picker is open (they own the keyboard).
    bool capturingInput() const { return mode_ != Mode::Idle; }

    // Shows a short message in the command bar area (e.g. "unknown context").
    void flash(std::string msg);

  private:
    enum class Mode { Idle, Command, Picker };
    enum class PickerKind { Context, Theme };

    Action runCommand(const Choices& choices);
    void openPicker(PickerKind kind);
    const std::vector<std::string>& items(const Choices& c) const;
    const std::string& current(const Choices& c) const;
    std::vector<std::size_t> filtered(const std::vector<std::string>& list) const;

    Mode mode_{Mode::Idle};
    PickerKind picker_{PickerKind::Context};
    std::string text_;        // command text, or picker filter
    std::size_t selected_{0}; // index into filtered list
    bool justOpened_{false};
    std::string message_;
    float messageTimer_{0.0f};
};

} // namespace k10s
