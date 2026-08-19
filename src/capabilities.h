// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#pragma once

#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <tuple>

class capabilities {
public:
    capabilities();
    ~capabilities();
    void query_window_size();
    std::optional<std::pair<int, int>> query_cursor_position() const;
    std::optional<bool> query_mode(const int mode) const;
    std::string query_setting(const std::string_view setting) const;
    std::string query_color_table() const;

    int width = 80;
    int height = 24;
    int level = 0;
    bool has_sixel = false;
    bool has_soft_fonts = false;
    bool has_full_cell_fonts = false;
    bool has_color = false;
    bool has_rectangle_ops = false;
    bool has_8bit = false;
    bool has_broken_sgr = false;
    bool has_pages = false;
    int terminal_id = 0;

private:
    void _query_device_attributes();
    void _query_terminal_id();
    static std::smatch _query(const char* pattern, const bool may_not_work);

    std::optional<bool> _original_decrpl;
};
