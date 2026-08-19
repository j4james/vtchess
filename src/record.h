// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

class screen;

class record {
public:
    record(screen& screen);
    void reset();
    void add(const std::string_view move);
    void game_over(const std::string_view reason);

private:
    void _render_title();
    void _render_move(const std::string& move, const int index, const bool clear) const;
    void _render_move(const std::string& move, const int y, const int x, const int attrs, const bool clear) const;
    void _scroll_up() const;

    screen& _screen;
    std::vector<std::string> _moves;
    const std::array<int, 4>& _attrs;
};
