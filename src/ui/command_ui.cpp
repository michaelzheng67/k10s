#include "ui/command_ui.hpp"

#include <algorithm>
#include <cctype>

namespace k10s {

namespace {

constexpr Color kPanel{20, 25, 30, 235};
constexpr Color kAccent{255, 170, 60, 255};
constexpr Color kDim{150, 160, 170, 255};

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool pressed(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }

bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

} // namespace

void CommandUi::flash(std::string msg) {
    message_ = std::move(msg);
    messageTimer_ = 3.0f;
}

const std::vector<std::string>& CommandUi::items(const Choices& c) const {
    return picker_ == PickerKind::Context ? c.contexts : c.themes;
}

const std::string& CommandUi::current(const Choices& c) const {
    return picker_ == PickerKind::Context ? c.context : c.theme;
}

std::vector<std::size_t> CommandUi::filtered(const std::vector<std::string>& list) const {
    std::vector<std::size_t> out;
    const std::string needle = lower(text_);
    for (std::size_t i = 0; i < list.size(); ++i)
        if (needle.empty() || lower(list[i]).find(needle) != std::string::npos) out.push_back(i);
    return out;
}

void CommandUi::openPicker(PickerKind kind) {
    mode_ = Mode::Picker;
    picker_ = kind;
    selected_ = 0;
    justOpened_ = true;
}

CommandUi::Action CommandUi::runCommand(const Choices& choices) {
    std::string cmd = text_;
    std::string arg;
    if (const auto sp = cmd.find(' '); sp != std::string::npos) {
        arg = cmd.substr(sp + 1);
        cmd = cmd.substr(0, sp);
    }
    text_.clear();
    mode_ = Mode::Idle;

    if (cmd == "q" || cmd == "quit" || cmd == "q!") return {ActionKind::Quit, {}};

    if (cmd == "ctx" || cmd == "context" || cmd == "contexts") {
        if (arg.empty()) {
            openPicker(PickerKind::Context);
            return {};
        }
        if (contains(choices.contexts, arg)) return {ActionKind::SwitchContext, arg};
        flash("unknown context: " + arg);
        return {};
    }
    if (cmd == "theme" || cmd == "themes" || cmd == "skin") {
        if (arg.empty()) {
            openPicker(PickerKind::Theme);
            return {};
        }
        if (contains(choices.themes, arg)) return {ActionKind::SwitchTheme, arg};
        flash("unknown theme: " + arg);
        return {};
    }
    if (!cmd.empty()) flash("unknown command: " + cmd);
    return {};
}

CommandUi::Action CommandUi::update(const Choices& choices, float dt) {
    if (messageTimer_ > 0.0f) messageTimer_ -= dt;

    // Collect typed characters (handles shift/layout correctly, unlike key codes).
    std::string typed;
    for (int c = GetCharPressed(); c > 0; c = GetCharPressed())
        if (c >= 32 && c < 127) typed.push_back(static_cast<char>(c));

    switch (mode_) {
    case Mode::Idle:
        if (typed.find(':') != std::string::npos) {
            mode_ = Mode::Command;
            text_.clear();
        }
        return {};

    case Mode::Command:
        if (IsKeyPressed(KEY_ESCAPE)) {
            mode_ = Mode::Idle;
            text_.clear();
            return {};
        }
        if (pressed(KEY_BACKSPACE)) {
            if (text_.empty()) mode_ = Mode::Idle;
            else text_.pop_back();
        }
        text_ += typed;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) return runCommand(choices);
        return {};

    case Mode::Picker: {
        if (IsKeyPressed(KEY_ESCAPE)) {
            mode_ = Mode::Idle;
            text_.clear();
            return {};
        }
        if (pressed(KEY_BACKSPACE) && !text_.empty()) text_.pop_back();
        if (!typed.empty()) {
            text_ += typed;
            selected_ = 0;
        }

        const auto& list = items(choices);
        const auto shown = filtered(list);
        if (shown.empty()) return {};
        if (justOpened_) { // start on the current entry
            justOpened_ = false;
            for (std::size_t i = 0; i < shown.size(); ++i)
                if (list[shown[i]] == current(choices)) selected_ = i;
        }
        if (pressed(KEY_DOWN)) selected_ = (selected_ + 1) % shown.size();
        if (pressed(KEY_UP)) selected_ = (selected_ + shown.size() - 1) % shown.size();
        selected_ = std::min(selected_, shown.size() - 1);

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            const std::string pick = list[shown[selected_]];
            const bool changed = pick != current(choices);
            const auto kind = picker_ == PickerKind::Context ? ActionKind::SwitchContext : ActionKind::SwitchTheme;
            mode_ = Mode::Idle;
            text_.clear();
            if (changed) return {kind, pick};
        }
        return {};
    }
    }
    return {};
}

void CommandUi::draw(const Choices& choices, int screenW, int screenH) const {
    constexpr int barH = 30;
    const int barY = screenH - barH;

    if (mode_ == Mode::Command) {
        DrawRectangle(0, barY, screenW, barH, kPanel);
        DrawText(":", 12, barY + 6, 20, kAccent);
        DrawText(text_.c_str(), 24, barY + 6, 20, RAYWHITE);
        const int caretX = 24 + MeasureText(text_.c_str(), 20) + 2;
        if (static_cast<int>(GetTime() * 2.0) % 2 == 0) DrawRectangle(caretX, barY + 6, 2, 20, RAYWHITE);
        constexpr const char* help = "ctx  |  ctx <name>  |  theme  |  q";
        DrawText(help, screenW - MeasureText(help, 14) - 12, barY + 9, 14, kDim);
    } else if (messageTimer_ > 0.0f) {
        DrawRectangle(0, barY, screenW, barH, kPanel);
        DrawText(message_.c_str(), 12, barY + 8, 16, kAccent);
    } else {
        constexpr const char* hint = "drag: pan   scroll: zoom   f: fit   : commands";
        DrawText(hint, screenW - MeasureText(hint, 14) - 12, barY + 8, 14, Color{255, 255, 255, 150});
    }

    if (mode_ != Mode::Picker) return;

    const auto& list = items(choices);
    const auto shown = filtered(list);
    constexpr int rowH = 28, panelW = 420;
    const int rows = std::max<int>(1, static_cast<int>(shown.size()));
    const int panelH = 70 + rows * rowH;
    const int px = (screenW - panelW) / 2, py = (screenH - panelH) / 2;

    DrawRectangle(0, 0, screenW, screenH, Color{0, 0, 0, 90});
    DrawRectangleRounded({static_cast<float>(px), static_cast<float>(py), static_cast<float>(panelW),
                          static_cast<float>(panelH)},
                         0.06f, 8, kPanel);
    DrawText(picker_ == PickerKind::Context ? "Contexts" : "Themes", px + 16, py + 12, 20, kAccent);
    DrawText(text_.empty() ? "type to filter" : text_.c_str(), px + 130, py + 15, 16,
             text_.empty() ? kDim : RAYWHITE);

    if (shown.empty()) {
        DrawText("no matches", px + 16, py + 50, 18, kDim);
        return;
    }
    for (std::size_t i = 0; i < shown.size(); ++i) {
        const int y = py + 48 + static_cast<int>(i) * rowH;
        const std::string& name = list[shown[i]];
        if (i == selected_) DrawRectangle(px + 8, y - 2, panelW - 16, rowH - 2, Color{255, 170, 60, 60});
        DrawText(name.c_str(), px + 20, y + 3, 18, RAYWHITE);
        if (name == current(choices)) DrawText("(current)", px + panelW - 100, y + 5, 14, kDim);
    }
}

} // namespace k10s
