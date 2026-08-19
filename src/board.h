// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#pragma once

#include "chess.h"

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

class screen;

using chess_board = chess::Board;
using chess_piece = chess::Piece;
using chess_piece_type = chess::PieceType;
using chess_square = chess::Square;
using chess_move = chess::Move;
using chess_moves = chess::Movelist;
using chess_result = chess::GameResultReason;

struct coord {
    coord() = default;
    coord(const int y, const int x) : x{x}, y{y} {}
    bool operator==(const coord& rhs) const = default;
    operator bool() const { return y != -1 && x != -1; };
    int y = -1;
    int x = -1;
};

class board {
public:
    board(screen& screen, const bool using_full_cell, const bool playing_white);
    void render_background();
    void render_background(const int y, const int x, const int y2, const int x2);
    void render_square(const coord board_pos, const bool focused = false, const std::optional<chess_piece> piece_override = {}) const;
    chess_piece operator[](const coord pos) const;
    chess_moves legal_moves() const;
    bool is_moveable(const coord pos) const;
    std::optional<chess_move> create_move(const std::string_view san) const;
    std::optional<chess_move> create_move(const coord source_pos, const coord target_pos) const;
    template <typename T>
    int preview_move(const chess_move move, T&& lambda);
    std::pair<coord, coord> visualize_move(const chess_move move, const std::chrono::duration<double> speed);
    std::string describe_move(const chess_move move) const;
    void apply_move(const chess_move move);
    int count_material(const chess_piece_type type) const;
    chess_result game_state() const;
    chess_result game_state(const chess_moves& moves, const int repetition_count = 1) const;

private:
    std::pair<char, int> _cell_content(const int cell_y, const int cell_x) const;
    std::pair<char, int> _cell_piece_content(const int cell_y, const int cell_x, const bool focused = false, const std::optional<chess_piece> piece_override = {}) const;
    std::pair<int, bool> _square_color(const chess_square square, const chess_piece piece, const bool focused) const;
    chess_square _square_at(const coord pos) const;
    coord _position_of(const chess_square square) const;

    const bool _using_full_cell = true;
    const bool _playing_white = true;
    const std::array<std::string_view, 8>& _glyphs;
    const std::array<int, 7>& _attrs;
    screen& _screen;
    chess_board _board;
};

template <typename T>
int board::preview_move(const chess_move move, T&& lambda)
{
    _board.makeMove(move);
    const auto score = lambda();
    _board.unmakeMove(move);
    return score;
}
