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

} // namespace

void CommandUi::flash(std::string msg) {
    message_ = std::move(msg);
    messageTimer_ = 3.0f;
}

std::vector<std::size_t> CommandUi::filtered(const std::vector<std::string>& contexts) const {
    std::vector<std::size_t> out;
    const std::string needle = lower(text_);
    for (std::size_t i = 0; i < contexts.size(); ++i)
        if (needle.empty() || lower(contexts[i]).find(needle) != std::string::npos)
            out.push_back(i);
    return out;
}

CommandUi::Action CommandUi::runCommand(const std::vector<std::string>& contexts) {
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
            mode_ = Mode::Picker;
            selected_ = 0;
            justOpened_ = true;
            return {};
        }
        if (std::find(contexts.begin(), contexts.end(), arg) != contexts.end())
            return {ActionKind::SwitchContext, arg};
        flash("unknown context: " + arg);
        return {};
    }
    if (!cmd.empty()) flash("unknown command: " + cmd);
    return {};
}

CommandUi::Action CommandUi::update(const std::vector<std::string>& contexts,
                                    const std::string& current, float dt) {
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
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) return runCommand(contexts);
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

        const auto items = filtered(contexts);
        if (items.empty()) return {};
        // Start on the current context when the picker first opens.
        if (justOpened_) {
            justOpened_ = false;
            for (std::size_t i = 0; i < items.size(); ++i)
                if (contexts[items[i]] == current) selected_ = i;
        }
        if (pressed(KEY_DOWN)) selected_ = (selected_ + 1) % items.size();
        if (pressed(KEY_UP)) selected_ = (selected_ + items.size() - 1) % items.size();
        selected_ = std::min(selected_, items.size() - 1);

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            const std::string pick = contexts[items[selected_]];
            mode_ = Mode::Idle;
            text_.clear();
            if (pick != current) return {ActionKind::SwitchContext, pick};
        }
        return {};
    }
    }
    return {};
}

void CommandUi::draw(const std::vector<std::string>& contexts, const std::string& current,
                     int screenW, int screenH) const {
    constexpr int barH = 30;
    const int barY = screenH - barH;

    if (mode_ == Mode::Command) {
        DrawRectangle(0, barY, screenW, barH, kPanel);
        DrawText(":", 12, barY + 6, 20, kAccent);
        DrawText(text_.c_str(), 24, barY + 6, 20, RAYWHITE);
        const int caretX = 24 + MeasureText(text_.c_str(), 20) + 2;
        if (static_cast<int>(GetTime() * 2.0) % 2 == 0)
            DrawRectangle(caretX, barY + 6, 2, 20, RAYWHITE);
        DrawText("ctx  |  ctx <name>  |  q", screenW - 250, barY + 9, 14, kDim);
    } else if (messageTimer_ > 0.0f) {
        DrawRectangle(0, barY, screenW, barH, kPanel);
        DrawText(message_.c_str(), 12, barY + 8, 16, kAccent);
    } else {
        DrawText("press : for commands", screenW - 190, barY + 8, 14, Color{255, 255, 255, 140});
    }

    if (mode_ != Mode::Picker) return;

    const auto items = filtered(contexts);
    constexpr int rowH = 28, panelW = 420;
    const int rows = std::max<int>(1, static_cast<int>(items.size()));
    const int panelH = 70 + rows * rowH;
    const int px = (screenW - panelW) / 2, py = (screenH - panelH) / 2;

    DrawRectangle(0, 0, screenW, screenH, Color{0, 0, 0, 90});
    DrawRectangleRounded({static_cast<float>(px), static_cast<float>(py),
                          static_cast<float>(panelW), static_cast<float>(panelH)},
                         0.06f, 8, kPanel);
    DrawText("Contexts", px + 16, py + 12, 20, kAccent);
    DrawText(text_.empty() ? "type to filter" : text_.c_str(), px + 130, py + 15, 16,
             text_.empty() ? kDim : RAYWHITE);

    if (items.empty()) {
        DrawText("no matches", px + 16, py + 50, 18, kDim);
        return;
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        const int y = py + 48 + static_cast<int>(i) * rowH;
        const std::string& name = contexts[items[i]];
        if (i == selected_)
            DrawRectangle(px + 8, y - 2, panelW - 16, rowH - 2, Color{255, 170, 60, 60});
        DrawText(name.c_str(), px + 20, y + 3, 18, RAYWHITE);
        if (name == current) DrawText("(current)", px + panelW - 100, y + 5, 14, kDim);
    }
}

} // namespace k10s
