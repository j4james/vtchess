// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#include "screen.h"

#include "capabilities.h"
#include "options.h"

#include <algorithm>
#include <iostream>
#include <string>

screen::screen(const capabilities& caps, const options& options)
    : _caps{caps}, _using_colors{options.color && caps.has_color}
{
    _ri = caps.has_8bit ? "\215" : "\033M";
    _csi = caps.has_8bit ? "\233" : "\033[";
    // Ideally we need 25 rows to display the full board, but on terminals that
    // only have 24, we prefer to trim the top of the board rather than the
    // bottom. We accomplish this with a virtual coordinate system in which the
    // y coordinate is offset by 1, and we avoid rendering that first row.
    _y_indent = std::max((caps.height - 25) / 2, 0) - (top_row_visible() ? 0 : 1);
    _x_indent = std::max((caps.width - 80) / 4, 0);
    // All of the rows need to have a double-width line rendition.
    sgr(sgr::normal);
    for (auto y = top_row_visible() ? 0 : 1; y < 25; y++) {
        cup(y, 0);
        _write("\033#6");
    }
}

screen::~screen()
{
    flush();
}

bool screen::using_colors() const
{
    return _using_colors;
}

void screen::sgr(const int attrs)
{
    if (attrs != _last_attrs) {
        // Level 1 terminals don't support resetting individual attributes,
        // so it's best to assume the state is unknown and apply everything.
        // We do the same for certain terminal emulators that are known to
        // have issues with cumulating SGR sequences.
        const auto apply_all = _last_attrs == sgr::unknown || _caps.level < 62 || _caps.has_broken_sgr;
        const auto changed_attrs = apply_all ? attrs : (attrs ^ _last_attrs);
        if (attrs == sgr::normal)
            _write(_csi, 'm');
        else {
            _write(_csi);
            auto need_semicolon = (apply_all && _last_attrs != sgr::normal);
            auto apply_attribute = [&](const int test, const int match, const int attr_value) {
                if ((changed_attrs & test) && (attrs & test) == match) {
                    if (need_semicolon) _write(';');
                    need_semicolon = true;
                    _write(attr_value);
                }
            };
            apply_attribute(sgr::bright, sgr::bright, 1);
            apply_attribute(sgr::bright, sgr::normal, 22);
            apply_attribute(sgr::underline, sgr::underline, 4);
            apply_attribute(sgr::underline, sgr::normal, 24);
            apply_attribute(sgr::blink, sgr::blink, 5);
            apply_attribute(sgr::blink, sgr::normal, 25);
            apply_attribute(sgr::reverse, sgr::reverse, 7);
            apply_attribute(sgr::reverse, sgr::normal, 27);
            if (_using_colors) {
                apply_attribute(sgr::fg, sgr::black_fg, 30);
                apply_attribute(sgr::fg, sgr::red_fg, 31);
                apply_attribute(sgr::fg, sgr::blue_fg, 34);
                apply_attribute(sgr::fg, sgr::white_fg, 37);
                apply_attribute(sgr::bg, sgr::black_bg, 40);
                apply_attribute(sgr::bg, sgr::red_bg, 41);
                apply_attribute(sgr::bg, sgr::blue_bg, 44);
                apply_attribute(sgr::bg, sgr::white_bg, 47);
            }
            _write('m');
        }
        _last_attrs = attrs;
    }
}

void screen::cup(const int y, const int x)
{
    // Relative positioning doesn't work reliably beyond column 40 because we
    // can't be sure whether the cursor has been clamped or not. So it's best
    // to treat the last position as unknown, and use absolute positioning.
    const auto unknown = _last_y == -1 || _last_x == -1 || _last_x >= 40;
    const auto abs_y = _abs_y(y);
    const auto abs_x = _abs_x(x);
    const auto diff_y = unknown ? 9999 : abs_y - _last_y;
    const auto diff_x = unknown ? 9999 : abs_x - _last_x;
    if (diff_y || diff_x) {
        if (abs(diff_y) > 2 && abs(diff_x) > 2)
            _write(_csi, abs_y, ';', abs_x, 'H');
        else {
            _move_x_relative(diff_x);
            _move_y_relative(diff_y);
        }
        _last_y = abs_y;
        _last_x = abs_x;
    }
}

void screen::select_charset(const charset cs)
{
    if (cs != _last_charset) {
        // Most of the time we'll be switching between ASCII in G0 and the Soft
        // Font in G1 with SI and SO. But sometimes we need to use the Special
        // Graphics set for line drawing, which we temporarily load into G0. So
        // if the last charset was `lines`, we must switch G0 back to ASCII to
        // restore our normal state. The reason for this whole rigmarole, when
        // it would have been simpler using G2, is because that's unfortunately
        // not supported in some terminal emulators.
        if (_last_charset == charset::lines || _last_charset == charset::unknown)
            _write("\033(B");
        switch (cs) {
            case charset::ascii:
                _write("\017");
                break;
            case charset::sprites:
                _write("\016");
                break;
            case charset::lines:
                _write("\033(0");
                if (_last_charset != charset::ascii)
                    _write("\017");
                break;
        }
        _last_charset = cs;
    }
}

void screen::show_cursor(const bool visible)
{
    _write(_csi, visible ? "?25h" : "?25l");
}

void screen::clear_to_end_of_line()
{
    _write(_csi, 'K');
}

void screen::erase_rect(const int y, const int x, const int y2, const int x2)
{
    if (_caps.has_rectangle_ops)
        _write(_csi, _abs_y(y), ';', _abs_x(x), ';', _abs_y(y2), ';', _abs_x(x2), ';', "$z");
    else {
        const auto width = x2 - x + 1;
        for (auto yy = y; yy <= y2; yy++) {
            cup(yy, x);
            _write(_csi, width, 'X');
        }
    }
}

bool screen::copy_rect(const int y, const int x, const int y2, const int x2, const int page,
    const int new_y, const int new_x, const int new_page)
{
    if (!_caps.has_rectangle_ops) return false;
    if (!_caps.has_pages && page * new_page != 1) return false;
    _write(_csi, _abs_y(y), ';', _abs_x(x), ';', _abs_y(y2), ';', _abs_x(x2), ';', page, ';', _abs_y(new_y), ';', _abs_x(new_x), ';', new_page, "$v");
    return true;
}

void screen::write(const std::string_view s)
{
    for (auto c : s) {
        _write(c);
        _last_x++;
    }
}

void screen::write(const std::string_view s, const int attrs)
{
    sgr(attrs);
    write(s);
}

void screen::write(const char c, const int attrs)
{
    write({&c, 1}, attrs);
}

void screen::flush()
{
    if (_buffer_index) {
        std::cout.write(&_buffer[0], _buffer_index);
        std::cout.flush();
        _buffer_index = 0;
    }
}

void screen::wait_until_done()
{
    flush();
    _caps.query_cursor_position();
}

bool screen::top_row_visible() const
{
    return _caps.height >= 25;
}

void screen::_move_y_relative(const int diff_y)
{
    if (diff_y == -1)
        _write(_ri);
    else if (diff_y == -2)
        _write(_ri, _ri);
    else if (diff_y == 1)
        _write('\v');
    else if (diff_y == 2)
        _write("\v\v");
    else if (diff_y > 0)
        _write(_csi, diff_y, 'B');
    else if (diff_y < 0)
        _write(_csi, -diff_y, 'A');
}

void screen::_move_x_relative(const int diff_x)
{
    if (diff_x == -1)
        _write('\b');
    else if (diff_x == -2)
        _write("\b\b");
    else if (diff_x == 1)
        _write(_csi, 'C');
    else if (diff_x > 0)
        _write(_csi, diff_x, 'C');
    else if (diff_x < 0)
        _write(_csi, -diff_x, 'D');
}

int screen::_abs_y(const int y) const
{
    return y + _y_indent + 1;
}

int screen::_abs_x(const int x) const
{
    return x + _x_indent + 1;
}

void screen::_write()
{
}

template <typename... Args>
void screen::_write(const int n, Args... args)
{
    _write(std::to_string(n));
    _write(args...);
}

template <typename... Args>
void screen::_write(const std::string_view s, Args... args)
{
    for (auto c : s)
        _write(c);
    _write(args...);
}

template <typename... Args>
void screen::_write(const char c, Args... args)
{
    _buffer[_buffer_index++] = c;
    _write(args...);
}
