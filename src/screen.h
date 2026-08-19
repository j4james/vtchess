// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#pragma once

#include <array>
#include <string_view>

class capabilities;
class options;

namespace sgr {
    constexpr int unknown = 0xFFFF;
    constexpr int normal = 0;
    constexpr int bright = 1;
    constexpr int underline = 2;
    constexpr int blink = 4;
    constexpr int reverse = 8;
    constexpr int fg = 3 << 4;
    constexpr int bg = 3 << 6;
    constexpr int white_fg = 0 << 4;
    constexpr int black_fg = 1 << 4;
    constexpr int red_fg = 2 << 4;
    constexpr int blue_fg = 3 << 4;
    constexpr int black_bg = 0 << 6;
    constexpr int white_bg = 1 << 6;
    constexpr int red_bg = 2 << 6;
    constexpr int blue_bg = 3 << 6;
}  // namespace sgr

enum class charset {
    unknown,
    ascii,
    lines,
    sprites
};

class screen {
public:
    screen(const capabilities& caps, const options& options);
    ~screen();
    bool using_colors() const;
    void sgr(const int attrs);
    void cup(const int y, const int x);
    void select_charset(const charset cs);
    void show_cursor(const bool visible = true);
    void clear_to_end_of_line();
    void erase_rect(const int y, const int x, const int y2, const int x2);
    bool copy_rect(const int y, const int x, const int y2, const int x2, const int page,
        const int new_y, const int new_x, const int new_page);
    void write(const std::string_view s);
    void write(const std::string_view s, const int attrs);
    void write(const char c, const int attrs);
    void flush();
    void wait_until_done();
    bool top_row_visible() const;

private:
    void _move_y_relative(const int diff_y);
    void _move_x_relative(const int diff_y);
    int _abs_y(const int y) const;
    int _abs_x(const int x) const;
    void _write();
    template <typename... Args>
    void _write(const int n, Args... args);
    template <typename... Args>
    void _write(const std::string_view s, Args... args);
    template <typename... Args>
    void _write(const char c, Args... args);

    const capabilities& _caps;
    const bool _using_colors;
    const char* _ri;
    const char* _csi;
    int _y_indent;
    int _x_indent;
    int _last_y = -1;
    int _last_x = -1;
    int _last_attrs = sgr::unknown;
    charset _last_charset = charset::unknown;
    std::array<char, 1024> _buffer = {};
    int _buffer_index = 0;
};
