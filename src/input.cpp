// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#include "input.h"

#include "os.h"
#include "screen.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace std::chrono_literals;
using uci = chess::uci;

namespace {

    constexpr auto UNVERIFIED = 0;
    constexpr auto VERIFIED = 1;
    constexpr auto PROMPT = 2;
    constexpr auto DIALOG_TEXT = 3;
    constexpr auto DIALOG_BORDER = 4;
    constexpr auto DIALOG_SHORTCUT = 5;

    constexpr auto color_attrs = std::array{
        sgr::black_fg | sgr::bright,                                 // UNVERIFIED
        sgr::white_fg,                                               // VERIFIED
        sgr::bright | sgr::blue_fg,                                  // PROMPT
        sgr::black_bg | sgr::white_fg | sgr::bright | sgr::reverse,  // DIALOG_TEXT
        sgr::white_bg | sgr::white_fg | sgr::bright | sgr::reverse,  // DIALOG_BORDER
        sgr::blue_bg | sgr::white_fg | sgr::bright | sgr::reverse,   // DIALOG_SHORTCUT
    };
    constexpr auto mono_attrs = std::array{
        sgr::normal,                    // UNVERIFIED
        sgr::bright,                    // VERIFIED
        sgr::bright,                    // PROMPT
        sgr::reverse,                   // DIALOG_TEXT
        sgr::reverse,                   // DIALOG_BORDER
        sgr::reverse | sgr::underline,  // DIALOG_SHORTCUT
    };

    constexpr auto UP_ARROW = 0x40000000;
    constexpr auto DOWN_ARROW = 0x40000001;
    constexpr auto RIGHT_ARROW = 0x40000002;
    constexpr auto LEFT_ARROW = 0x40000003;
    constexpr auto SPACE = 0x40000004;
    constexpr auto ENTER = 0x40000005;
    constexpr auto BKSP = 0x40000006;

    int get_key()
    {
        for (;;) {
            auto ch = os::getch();
            if (ch == 27) {
                ch = os::getch();
                if (ch == '[' || ch == 'O') {
                    switch (os::getch()) {
                        case 'A': return UP_ARROW;
                        case 'B': return DOWN_ARROW;
                        case 'C': return RIGHT_ARROW;
                        case 'D': return LEFT_ARROW;
                    }
                } else {
                    continue;
                }
            } else if (ch == 127 || ch == 8) {
                return BKSP;
            } else if (ch == 32) {
                return SPACE;
            } else if (ch == 13) {
                return ENTER;
            } else if (ch == 3) {
                throw std::exception{};
            }
            return ch;
        }
    }

    constexpr auto TOP = 24;
    constexpr auto LEFT = 27;

}  // namespace

input::input(screen& screen, board& board)
    : _screen{screen}, _board{board},
      _attrs{screen.using_colors() ? color_attrs : mono_attrs}
{
    _render_prompt();
}

chess_move input::get_move()
{
    _focused_pos = {};
    _selected_pos = {};
    _selected_piece = {};

    if (_auto_focus) {
        // We add a bit of a delay before auto-focusing our piece so it's more
        // clearly separated from the computer's last movement.
        std::this_thread::sleep_for(500ms);
        _focused_pos = _suggested_focus_pos();
        _board.render_square(_focused_pos, true);
    }

    for (;;) {
        _screen.cup(TOP, LEFT + _buffer.length());
        _screen.show_cursor();
        _screen.flush();
        const auto key = get_key();
        _screen.show_cursor(false);
        switch (key) {
            case UP_ARROW:
                if (!_focused_pos || _focused_pos.y > 0)
                    _move_focused_pos(-1, 0);
                break;
            case DOWN_ARROW:
                if (!_focused_pos || _focused_pos.y < 7)
                    _move_focused_pos(+1, 0);
                break;
            case RIGHT_ARROW:
                if (!_focused_pos || _focused_pos.x < 7)
                    _move_focused_pos(0, +1);
                break;
            case LEFT_ARROW:
                if (!_focused_pos || _focused_pos.x > 0)
                    _move_focused_pos(0, -1);
                break;
            case SPACE:
                // If you're using the arrow keys to make your move, then both
                // Space and Enter can be used to select a piece. But if you're
                // typing in the move as a SAN string (i.e. the buffer isn't
                // empty), then only Enter will complete the move.
                if (!_buffer.empty())
                    break;
                [[fallthrough]];
            case ENTER:
                if (_quit_requested())
                    throw std::exception{};
                if (_buffer.empty()) {
                    if (!_focused_pos)
                        _move_focused_pos(0, 0);
                    else if (!_selected_pos)
                        _select_focused_pos();
                    else if (_selected_pos == _focused_pos)
                        _undo_selection();
                }
                if (_selected_pos && _focused_pos && _selected_pos != _focused_pos) {
                    const auto move = _move_from_selection();
                    if (move)
                        return *move;
                    else
                        _flash_focused_pos();
                }
                break;
            case BKSP:
                if (_buffer.empty())
                    _undo_selection();
                else
                    _undo_input();
                break;
            default:
                if (std::isalnum(key) || key == '-' || key == '=')
                    _add_to_input(key);
                break;
        }
    }
}

char input::popup_dialog(const std::string_view message)
{
    auto choices = std::unordered_set<int>{};
    auto lines = std::vector<std::string>{};
    auto width = 0;
    for (auto start = 0;;) {
        const auto lf = message.find('\n', start);
        const auto line = message.substr(start, lf - start);
        const auto shortcuts = std::count(line.begin(), line.end(), '&');
        lines.emplace_back(line);
        width = std::max<int>(width, line.length() - shortcuts + 4);
        if (lf == std::string::npos) break;
        start = lf + 1;
    }

    // On terminals that support multiple pages, we can save the the background
    // by copying it to another page, and then copy it back when the dialog is
    // closed. On lower level terminals we're forced to redraw the background.
    const auto height = lines.size() + 4;
    const auto y = (24 - height) / 2;
    const auto x = (26 - width) / 2;
    const auto y2 = y + height - 1;
    const auto x2 = x + width - 1;
    _screen.copy_rect(y, x, y2, x2, 1, y, x, 2);

    // Note that the line drawing character set uses characters in the range
    // `j` to `x` to represent border lines. So to produce the pattern on the
    // left (for the dialog border), we use the characters on the right.
    // ┌────┐  lqqqqk
    // │    │  x    x
    // └────┘  mqqqqj
    auto yy = y;
    _screen.show_cursor(false);
    _screen.select_charset(charset::lines);
    _screen.sgr(_attrs[DIALOG_BORDER]);
    _screen.cup(yy++, x);
    _screen.write('l' + std::string(width - 2, 'q') + 'k');
    _screen.cup(yy++, x);
    _screen.write("x");
    _screen.select_charset(charset::ascii);
    _screen.write(std::string(width - 2, ' '));
    _screen.select_charset(charset::lines);
    _screen.write("x");
    for (auto line : lines) {
        _screen.select_charset(charset::lines);
        _screen.sgr(_attrs[DIALOG_BORDER]);
        _screen.cup(yy++, x);
        _screen.write("x");
        _screen.select_charset(charset::ascii);
        _screen.write(" ");
        auto padding = width - 3;
        for (auto i = 0; i < line.length(); i++) {
            if (line[i] == '&') {
                _screen.sgr(_attrs[DIALOG_SHORTCUT]);
                _screen.write(line.substr(++i, 1));
                choices.insert(line[i]);
            } else {
                _screen.sgr(_attrs[DIALOG_TEXT]);
                _screen.write(line.substr(i, 1));
            }
            padding--;
        }
        _screen.write(std::string(padding, ' '));
        _screen.select_charset(charset::lines);
        _screen.sgr(_attrs[DIALOG_BORDER]);
        _screen.write("x");
    }
    _screen.cup(yy++, x);
    _screen.write("x");
    _screen.select_charset(charset::ascii);
    _screen.write(std::string(width - 2, ' '));
    _screen.select_charset(charset::lines);
    _screen.write("x");
    _screen.cup(yy, x);
    _screen.write('m' + std::string(width - 2, 'q') + 'j');
    _screen.flush();

    for (;;) {
        auto ch = get_key();
        if (ch >= 'a' && ch <= 'z')
            ch = std::toupper(ch);
        if (choices.contains(ch)) {
            if (!_screen.copy_rect(y, x, y2, x2, 2, y, x, 1))
                _board.render_background(y, x, y2, x2);
            _screen.flush();
            return ch;
        }
    }
}

void input::press_any_key()
{
    // Wait a second and then flush the keyboard buffer to make sure we aren't
    // getting an accidental keypress before the user realises the game has
    // ended (this happened to me a lot). The wait_until_done call queries the
    // terminal for a DSR-CPR report, which eats any other input in the buffer.
    std::this_thread::sleep_for(2s);
    _screen.wait_until_done();
    _screen.select_charset(charset::ascii);
    _screen.cup(TOP, LEFT - 1);
    _screen.write("Press any key", _attrs[UNVERIFIED] | sgr::blink);
    _screen.flush();
    get_key();
    _render_prompt();
}

bool input::_quit_requested() const
{
    return _buffer == "quit" || _buffer == "exit";
}

void input::_add_to_input(const char ch)
{
    if (_buffer.length() < 11) {
        _undo_focus();
        const auto attr = _attrs[_buffer_verified ? VERIFIED : UNVERIFIED];
        _screen.select_charset(charset::ascii);
        _screen.cup(TOP, LEFT + _buffer.length());
        _screen.write(ch, attr);
        _buffer += ch;
        _verify_input();
    }
}

void input::_undo_input()
{
    if (!_buffer.empty()) {
        _undo_focus();
        _buffer.pop_back();
        _screen.select_charset(charset::ascii);
        _screen.cup(TOP, LEFT + _buffer.length());
        _screen.write(' ', _attrs[UNVERIFIED]);
        _verify_input();
    }
}

void input::_verify_input()
{
    // If the user has entered a valid SAN string, we redraw the input buffer
    // in the verified color, and display the pending move on the board.
    const auto move = _board.create_move(_buffer);
    if (move || _quit_requested()) {
        if (!_buffer_verified) {
            _buffer_verified = true;
            _screen.cup(TOP, LEFT);
            _screen.write(_buffer, _attrs[VERIFIED]);
        }
        if (move) _select_from_move(*move);
    } else {
        if (_buffer_verified) {
            _buffer_verified = false;
            _screen.cup(TOP, LEFT);
            _screen.write(_buffer, _attrs[UNVERIFIED]);
        }
    }
}

void input::_clear_input()
{
    if (!_buffer.empty()) {
        _buffer = {};
        _buffer_verified = false;
        _screen.sgr(_attrs[UNVERIFIED]);
        _screen.cup(TOP, LEFT);
        _screen.clear_to_end_of_line();
    }
}

coord input::_suggested_focus_pos() const
{
    // If nothing has been previously focused (it's the start of the game), we
    // focus on the king's pawn. Otherwise we'll focus on the closest piece to
    // the last focused position (not necessarily the last position, because
    // that piece may have been captured).
    if (!_last_focused_pos) return {6, 4};
    auto best_distance = 9999;
    auto best_pos = coord{};
    for (auto y = 0; y < 8; y++)
        for (auto x = 0; x < 8; x++) {
            const auto pos = coord{y, x};
            if (_board.is_moveable(pos)) {
                const auto dy = pos.y - _last_focused_pos.y;
                const auto dx = pos.x - _last_focused_pos.x;
                const auto distance = dy * dy + dx * dx;
                if (distance < best_distance) {
                    best_distance = distance;
                    best_pos = pos;
                }
            }
        }
    return best_pos;
}

void input::_move_focused_pos(const int dy, const int dx)
{
    _clear_input();
    if (!_focused_pos)
        _focused_pos = _suggested_focus_pos();
    else {
        if (_focused_pos == _selected_pos)
            _board.render_square(_focused_pos, false, chess_piece::NONE);
        else
            _board.render_square(_focused_pos, false);
        _focused_pos.y += dy;
        _focused_pos.x += dx;
    }
    _board.render_square(_focused_pos, true, _selected_piece);
}

void input::_flash_focused_pos() const
{
    if (_focused_pos) {
        _board.render_square(_focused_pos, false, _selected_piece);
        std::this_thread::sleep_for(50ms);
        _board.render_square(_focused_pos, true, _selected_piece);
        std::this_thread::sleep_for(50ms);
        _board.render_square(_focused_pos, false, _selected_piece);
        std::this_thread::sleep_for(50ms);
        _board.render_square(_focused_pos, true, _selected_piece);
    }
}

void input::_select_focused_pos()
{
    if (_board.is_moveable(_focused_pos)) {
        _selected_pos = _focused_pos;
        _selected_piece = _board[_selected_pos];
        _board.render_square(_selected_pos, false);
        std::this_thread::sleep_for(100ms);
        _board.render_square(_selected_pos, true);
    } else {
        _flash_focused_pos();
    }
}

void input::_select_from_move(const chess_move move)
{
    const auto [from, to] = _board.visualize_move(move, 250ms);
    _focused_pos = to;
    _selected_pos = from;
    _selected_piece = _board[_selected_pos];
}

std::optional<chess_move> input::_move_from_selection()
{
    const auto move = _board.create_move(_selected_pos, _focused_pos);
    if (move) {
        _last_focused_pos = _focused_pos;
        _auto_focus = _buffer.empty();
        _clear_input();
    }
    return move;
}

void input::_undo_selection()
{
    if (_selected_pos) {
        _clear_input();
        if (_selected_pos == _focused_pos)
            _flash_focused_pos();
        else {
            _board.render_square(_focused_pos, false);
            _focused_pos = _selected_pos;
            _board.render_square(_focused_pos, true);
        }
        _selected_pos = {};
        _selected_piece = {};
    }
}

void input::_undo_focus()
{
    if (_focused_pos) {
        _board.render_square(_focused_pos, false);
        if (_selected_pos && _selected_pos != _focused_pos)
            _board.render_square(_selected_pos, false);
        _last_focused_pos = _selected_pos ? _selected_pos : _focused_pos;
        _focused_pos = {};
        _selected_pos = {};
        _selected_piece = {};
    }
}

void input::_render_prompt() const
{
    _screen.cup(TOP, LEFT - 1);
    _screen.select_charset(charset::sprites);
    _screen.write('*', _attrs[PROMPT]);
    _screen.clear_to_end_of_line();
    _screen.flush();
}
