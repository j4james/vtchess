// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#pragma once

#include "board.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

class input {
public:
    input(screen& screen, board& board);
    chess_move get_move();
    char popup_dialog(const std::string_view message);
    void press_any_key();

private:
    bool _quit_requested() const;
    void _add_to_input(const char ch);
    void _undo_input();
    void _verify_input();
    void _clear_input();
    coord _suggested_focus_pos() const;
    void _move_focused_pos(const int dy, const int dx);
    void _flash_focused_pos() const;
    void _select_focused_pos();
    void _select_from_move(const chess_move move);
    std::optional<chess_move> _move_from_selection();
    void _undo_selection();
    void _undo_focus();
    void _render_prompt() const;

    screen& _screen;
    board& _board;
    const std::array<int, 6>& _attrs;
    std::string _buffer;
    bool _buffer_verified = false;
    bool _auto_focus = false;
    coord _focused_pos;
    coord _last_focused_pos;
    coord _selected_pos;
    std::optional<chess_piece> _selected_piece;
};
