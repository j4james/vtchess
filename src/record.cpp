// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#include "record.h"

#include "screen.h"

namespace {

    constexpr auto TITLE = 0;
    constexpr auto BLACK = 1;
    constexpr auto WHITE = 2;
    constexpr auto GAME_OVER = 3;

    constexpr auto color_attrs = std::array{
        sgr::bright | sgr::blue_fg,    // TITLE
        sgr::bright | sgr::black_fg,   // BLACK
        sgr::white_fg,                 // WHITE
        sgr::white_fg | sgr::blue_bg,  // GAME_OVER
    };
    constexpr auto mono_attrs = std::array{
        sgr::normal,                 // TITLE
        sgr::normal,                 // BLACK
        sgr::bright,                 // WHITE
        sgr::bright | sgr::reverse,  // GAME_OVER
    };

}  // namespace

record::record(screen& screen)
    : _screen{screen}, _attrs{screen.using_colors() ? color_attrs : mono_attrs}
{
    _render_title();
}

void record::reset()
{
    const auto top = _screen.top_row_visible() ? 0 : 1;
    _screen.sgr(sgr::normal);
    _screen.erase_rect(top + 3, 25, 22, 40);
    _moves.clear();
}

void record::add(const std::string_view move)
{
    const auto max_moves = _screen.top_row_visible() ? 40 : 38;
    if (_moves.size() >= max_moves) {
        _moves.erase(_moves.begin(), _moves.begin() + 2);
        _scroll_up();
    }
    const auto index = _moves.size();
    _moves.emplace_back(move);
    _render_move(_moves.back(), index, false);
}

void record::game_over(const std::string_view reason)
{
    // We want to leave a bit of a gap between the last recorded move and the
    // game over message, so we pad the list with a few blank entries.
    const auto padding = _moves.size() % 2 ? 4 : 3;
    for (auto i = 0; i < padding; i++)
        add("");
    const auto message = ' ' + std::string{reason} + ' ';
    _screen.write(message, _attrs[GAME_OVER]);
    _screen.flush();
}

void record::_render_title()
{
    const auto top = _screen.top_row_visible() ? 0 : 1;
    _screen.sgr(_attrs[TITLE]);
    _screen.select_charset(charset::lines);
    _screen.cup(top, 26);
    _screen.write("qqqqqqqqqqqqq");
    _screen.select_charset(charset::sprites);
    _screen.cup(top + 1, 26);
    // This looks like gibberish, but these characters represent glyphs for
    // the letters `V T C H E S S` in our soft font. And the `q` character
    // used above and below is a horizontal line in the line drawing set.
    _screen.write("\\ ! / _ Z z z");
    _screen.select_charset(charset::lines);
    _screen.cup(top + 2, 26);
    _screen.write("qqqqqqqqqqqqq");
    _screen.cup(23, 26);
    _screen.write("qqqqqqqqqqqqq");
    _screen.flush();
}

void record::_render_move(const std::string& move, const int index, const bool clear) const
{
    const auto y = index / 2;
    const auto x = index % 2 * 7;
    const auto attrs = index % 2 ? _attrs[BLACK] : _attrs[WHITE];
    _render_move(move, y, x, attrs, clear);
}

void record::_render_move(const std::string& move, const int y, const int x, const int attrs, const bool clear) const
{
    const auto top = _screen.top_row_visible() ? 3 : 4;
    _screen.select_charset(charset::ascii);
    _screen.cup(top + y, 26 + x);
    _screen.sgr(attrs);
    if (clear) _screen.clear_to_end_of_line();
    _screen.write(move);
    _screen.flush();
}

void record::_scroll_up() const
{
    const auto top = _screen.top_row_visible() ? 3 : 4;
    const auto height = _moves.size() / 2;
    if (!_screen.copy_rect(top + 1, 26, 22, 40, 1, top, 26, 1)) {
        for (auto i = 0; i < _moves.size(); i++)
            _render_move(_moves[i], i, i % 2 == 0);
    }
    _screen.sgr(sgr::normal);
    _screen.cup(22, 26);
    _screen.clear_to_end_of_line();
}
