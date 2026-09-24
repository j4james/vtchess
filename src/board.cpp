// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#include "board.h"

#include "screen.h"

#include <thread>

using namespace std::chrono_literals;
using uci = chess::uci;

using chess_color = chess::Color;
using chess_file = chess::File;
using chess_rank = chess::Rank;
using movegen = chess::movegen;

namespace {

    constexpr auto BOARD_COLOR = 0;
    constexpr auto WHITE_PIECE_ON_WHITE = 1;
    constexpr auto WHITE_PIECE_ON_BLACK = 2;
    constexpr auto BLACK_PIECE_ON_WHITE = 3;
    constexpr auto BLACK_PIECE_ON_BLACK = 4;
    constexpr auto FOCUS_ADDS = 5;
    constexpr auto FOCUS_REMOVES = 6;

    constexpr auto color_attrs = std::array{
        sgr::red_fg | sgr::red_bg | sgr::bright,    // BOARD_COLOR
        sgr::white_bg | sgr::red_fg | sgr::bright,  // WHITE_PIECE_ON_WHITE
        sgr::white_bg | sgr::red_fg,                // WHITE_PIECE_ON_BLACK
        sgr::black_bg | sgr::red_fg | sgr::bright,  // BLACK_PIECE_ON_WHITE
        sgr::black_bg | sgr::red_fg,                // BLACK_PIECE_ON_BLACK
        sgr::blue_fg,                               // FOCUS_ADDS
        sgr::fg,                                    // FOCUS_REMOVES
    };
    constexpr auto mono_attrs = std::array{
        sgr::bright,                 // BOARD_COLOR
        sgr::reverse | sgr::bright,  // WHITE_PIECE_ON_WHITE
        sgr::reverse | sgr::bright,  // WHITE_PIECE_ON_BLACK
        sgr::bright,                 // BLACK_PIECE_ON_WHITE
        sgr::normal,                 // BLACK_PIECE_ON_BLACK
        sgr::normal,                 // FOCUS_ADDS
        sgr::bright,                 // FOCUS_REMOVES
    };

    constexpr auto RANK_INDICATORS = 6;
    constexpr auto FILE_INDICATORS = 7;

    constexpr auto white_board = std::array<std::string_view, 8>{
        "9.Y:y;",   // Top border
        "[ | |]",   // Odd rows
        "1-+-+(",   // Odd row separators
        "{ | |}",   // Even rows
        "1-+-+)",   // Even row separators
        "'^#~#@",   // Bottom border
        "7654321",  // Rank indicators
        "#$%&<=>",  // File indicators
    };
    constexpr auto black_board = std::array<std::string_view, 8>{
        "0:y.Y,",   // Top border
        "{ | |}",   // Odd rows
        "1-+-+)",   // Odd row separators
        "[ | |]",   // Even rows
        "1-+-+(",   // Even row separators
        "`~#^#\"",  // Bottom border
        "2345678",  // Rank indicators
        "?>=<&%$",  // File indicators
    };
    constexpr auto vt200_board = std::array<std::string_view, 8>{
        ",.Y:y;",  // Top border
        "[ $ |]",  // Odd rows
        ">=+-&(",  // Odd row separators
        "{ | $}",  // Even rows
        "<-&=+)",  // Even row separators
        "'^#~%`",  // Bottom border
        "",        // Unused
        "",        // Unused
    };

    constexpr auto solid_pieces = std::array<std::string_view, 7>{
        "UVWX",  // PAWN
        "QRST",  // KNIGHT
        "MNOP",  // BISHOP
        "IJKL",  // ROOK
        "EFGH",  // QUEEN
        "ABCD",  // KING
        "    ",  // White square with no piece
    };
    constexpr auto hollow_pieces = std::array<std::string_view, 7>{
        "uvwx",  // PAWN
        "qrst",  // KNIGHT
        "mnop",  // BISHOP
        "ijkl",  // ROOK
        "efgh",  // QUEEN
        "abcd",  // KING
        "@@@@",  // Black square with no piece (VT200 only)
    };

}  // namespace

board::board(screen& screen, const bool using_full_cell, const bool playing_white)
    : _screen{screen}, _using_full_cell{using_full_cell}, _playing_white{playing_white},
      _glyphs{using_full_cell ? (playing_white ? white_board : black_board) : vt200_board},
      _attrs{screen.using_colors() ? color_attrs : mono_attrs}
{
}

void board::render_background()
{
    const auto top = _screen.top_row_visible() ? 0 : 1;
    render_background(top, 0, 24, 24);
    _screen.wait_until_done();
}

void board::render_background(const int y, const int x, const int y2, const int x2)
{
    _screen.select_charset(charset::sprites);
    for (auto cell_y = y; cell_y <= y2; cell_y++) {
        _screen.cup(cell_y, x);
        for (auto cell_x = x; cell_x <= x2; cell_x++) {
            const auto [ch, color] = _cell_content(cell_y, cell_x);
            _screen.write(ch, color);
        }
        _screen.flush();
    }
}

void board::render_square(const coord pos, const bool focused, const std::optional<chess_piece> piece_override) const
{
    const auto y = pos.y * 3 + 1;
    const auto x = pos.x * 3 + 1;
    _screen.select_charset(charset::sprites);
    for (auto cell_y = y; cell_y < y + 2; cell_y++) {
        _screen.cup(cell_y, x);
        for (auto cell_x = x; cell_x < x + 2; cell_x++) {
            const auto [ch, color] = _cell_piece_content(cell_y, cell_x, focused, piece_override);
            _screen.write(ch, color);
        }
    }
    _screen.flush();
}

chess_piece board::operator[](const coord pos) const
{
    const auto square = _square_at(pos);
    return _board.at(square);
}

chess_moves board::legal_moves() const
{
    // Instead of asking for all legal moves, we get the capturing moves first
    // and add the quiet moves to end of that list. This will be the order in
    // which the game engine evaluates the moves, so it's more likely to find
    // the best move earlier, and less of the game tree needs to be searched.
    // This can makes a significant difference to the evaluation time.
    auto capture_moves = chess_moves{};
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(capture_moves, _board);
    auto quiet_moves = chess_moves{};
    movegen::legalmoves<movegen::MoveGenType::QUIET>(quiet_moves, _board);
    for (auto quiet_move : quiet_moves)
        capture_moves.add(quiet_move);
    return capture_moves;
}

bool board::is_moveable(const coord pos) const
{
    const auto piece = (*this)[pos];
    return (piece.color() == _board.sideToMove());
}

std::optional<chess_move> board::create_move(const std::string_view san) const
{
    try {
        const auto parse_san = [this](const auto san) {
            if (san.empty() || san.ends_with('-'))
                throw uci::SanParseError("");
            return uci::parseSan(_board, san);
        };
        const auto move = parse_san(san);
        try {
            // The parseSan function will happily accept some SAN strings with
            // extra characters on the end, so if we can parse a shorter version
            // of the string with the same result, then the long version isn't
            // really valid.
            const auto short_san = san.substr(0, san.length() - 1);
            const auto short_move = parse_san(short_san);
            if (short_move == move) return {};
            return move;
        } catch (...) {
            return move;
        }
    } catch (...) {
        return {};
    }
}

std::optional<chess_move> board::create_move(const coord source_pos, const coord target_pos) const
{
    const auto source = _square_at(source_pos);
    const auto target = _square_at(target_pos);
    const auto type = _board.at(source).type();
    const auto xoffset = abs(target.file() - source.file());
    const auto first_rank = _board.sideToMove() == chess_color::WHITE ? chess_rank::RANK_1 : chess_rank::RANK_8;
    const auto last_rank = _board.sideToMove() == chess_color::WHITE ? chess_rank::RANK_8 : chess_rank::RANK_1;
    const auto construct_move = [&]() {
        // If a pawn is moving to the last row, it must be a promotion.
        if (type == chess_piece_type::PAWN && target.rank() == last_rank)
            return chess_move::make<chess_move::PROMOTION>(source, target, chess_piece_type::QUEEN);
        // If a pawn is moving horizontally onto an empty square, it must be en passant.
        if (type == chess_piece_type::PAWN && xoffset == 1 && _board.at(target) == chess_piece::NONE)
            return chess_move::make<chess_move::ENPASSANT>(source, target);
        // If a king is moving horizontally by two squares, it must be attempting to castle.
        if (type == chess_piece_type::KING && xoffset == 2 && target.rank() == first_rank) {
            // But to trigger castling, we need to set the target to the rook position.
            const auto rook_file = target.file() > source.file() ? chess_file::FILE_H : chess_file::FILE_A;
            const auto rook_square = chess_square{first_rank, rook_file};
            return chess_move::make<chess_move::CASTLING>(source, rook_square);
        }
        return chess_move::make(source, target);
    };
    const auto move = construct_move();
    return _board.isLegal(move) ? std::make_optional(move) : std::nullopt;
}

std::pair<coord, coord> board::visualize_move(const chess_move move, const std::chrono::duration<double> speed)
{
    const auto source_pos = _position_of(move.from());
    auto target_pos = _position_of(move.to());
    if (move.typeOf() == chess_move::CASTLING) {
        const auto king_side = move.to() > move.from();
        const auto move_color = _board.sideToMove();
        const auto king_square = chess_square::castling_king_square(king_side, move_color);
        target_pos = _position_of(king_square);
    }
    render_square(source_pos, true);
    std::this_thread::sleep_for(speed);
    render_square(source_pos, false, chess_piece::NONE);
    render_square(target_pos, true, _board.at(move.from()));
    return {source_pos, target_pos};
}

std::string board::describe_move(const chess_move move) const
{
    return uci::moveToSan(_board, move);
}

void board::apply_move(const chess_move move)
{
    const auto move_color = _board.sideToMove();
    _board.makeMove(move);
    render_square(_position_of(move.to()));
    if (move.typeOf() == chess_move::ENPASSANT) {
        // For an en-passant attack we need to redraw the piece that was taken.
        const auto taken_square = chess_square{move.from().rank(), move.to().file()};
        render_square(_position_of(taken_square));
    }
    if (move.typeOf() == chess_move::CASTLING) {
        // When castling, the target_pos is the original rook positions, which
        // has already been rendered now. However, we will still need to render
        // the new rook position, and also refresh the king position.
        const auto king_side = move.to() > move.from();
        const auto rook_square = chess_square::castling_rook_square(king_side, move_color);
        const auto king_square = chess_square::castling_king_square(king_side, move_color);
        render_square(_position_of(rook_square));
        render_square(_position_of(king_square));
    }
}

int board::count_material(const chess_piece_type type) const
{
    const auto white_count = _board.pieces(type, chess_color::WHITE).count();
    const auto black_count = _board.pieces(type, chess_color::BLACK).count();
    if (_board.sideToMove() == chess_color::WHITE)
        return black_count - white_count;
    else
        return white_count - black_count;
}

chess_result board::game_state() const
{
    auto moves = chess_moves{};
    movegen::legalmoves(moves, _board);
    // I'm not 100% sure I know what I'm doing here, but the game_state method
    // below calculates whether a game has reached a draw because of a threefold
    // repetition based on the repetition_count parameter. This parameter needs
    // to be 2 when we're determining whether the game is over, but when called
    // from the engine class (calculating the computer's next move), the default
    // is 1. This is what the disservin chess library says we should be doing.
    // I think this helps the computer avoid the situation where the human's
    // next move could trigger the stale mate.
    return game_state(moves, 2);
}

chess_result board::game_state(const chess_moves& moves, const int repetition_count) const
{
    // This is the equivalent of the isGameOver method in the disservin library,
    // but without having to recalculate the legal moves. When evaluting the
    // state from the engine class, we'll already have that data.
    if (_board.isHalfMoveDraw()) {
        if (moves.empty() && _board.inCheck())
            return chess_result::CHECKMATE;
        else
            return chess_result::FIFTY_MOVE_RULE;
    } else if (_board.isInsufficientMaterial())
        return chess_result::INSUFFICIENT_MATERIAL;
    else if (_board.isRepetition(repetition_count))
        return chess_result::THREEFOLD_REPETITION;
    else if (moves.empty())
        return _board.inCheck() ? chess_result::CHECKMATE : chess_result::STALEMATE;
    else
        return chess_result::NONE;
}

std::pair<char, int> board::_cell_content(const int cell_y, const int cell_x) const
{
    // The glyph table is a squished representation of the board, compressing
    // the 25x25 characters down to a set of 6x6. This glyph index map helps
    // translate screen coordinates into the appropriate position in that glyph
    // table. Along both axes we have a border character at each end (0/5),
    // and between them we have odd squares and separators (1/2), alternating
    // with even squares and separators (3/4).
    const auto glyph_index_map = std::array{0, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 5};
    const auto col_index = glyph_index_map[(cell_x * 2 + 1) / 3];
    const auto row_index = glyph_index_map[(cell_y * 2 + 1) / 3];
    auto ch = _glyphs[row_index][col_index];

    // The space character represents an area of the board where a chess piece
    // may need to be rendered, so we handle that in a separate method.
    if (ch == ' ')
        return _cell_piece_content(cell_y, cell_x);
    else {
        if (_using_full_cell) {
            const auto col = cell_x / 3;
            const auto row = cell_y / 3;
            const auto white_square = (col % 2 == row % 2);
            const auto border = (cell_y == 0 || cell_y == 24 || cell_x == 0 || cell_x == 24);

            // In full-cell fonts, the glyphs that represent the boundaries
            // between squares are shared for both colors, so we need to
            // reverse the attributes when we're on the boundary of a white
            // square. The same applies to the border glyphs that indicate
            // the rank and file, so again we need to reverse the attributes
            // of the border area when playing as white.
            auto attr = _attrs[BOARD_COLOR];
            if ((!border && white_square) || (border && _playing_white))
                attr |= sgr::reverse;

            // The glyph table returns `1` for all rank indicators, and `#`
            // for all file indicators, so we need to replace those with the
            // actual rank and file based on the current row and column.
            if (ch == '1')
                ch = _glyphs[RANK_INDICATORS][row - 1];
            else if (ch == '#')
                ch = _glyphs[FILE_INDICATORS][col - 1];

            return {ch, attr};
        } else {
            // The VT200 text font has unique glyphs for all the boundary
            // characters, and there are no rank and file indicators, so
            // everything is reverse, and we don't need to do any character
            // replacement for the rank and file.
            const auto attr = _attrs[BOARD_COLOR] | sgr::reverse;
            return {ch, attr};
        }
    }
}

std::pair<char, int> board::_cell_piece_content(const int cell_y, const int cell_x, const bool focused, const std::optional<chess_piece> piece_override) const
{
    const auto square = _square_at({cell_y / 3, cell_x / 3});
    const auto piece = piece_override.value_or(_board.at(square));
    const auto [color, hollow] = _square_color(square, piece, focused);
    const auto glyph_set = hollow ? hollow_pieces : solid_pieces;
    const auto glyphs = glyph_set[piece.type()];
    const auto glyph_index = (cell_y % 3 - 1) * 2 + (cell_x % 3 - 1);
    return {glyphs[glyph_index], color};
}

std::pair<int, bool> board::_square_color(const chess_square square, const chess_piece piece, const bool focused) const
{
    const auto white_piece = piece.color() == chess_color::WHITE;
    const auto white_square = square.is_light();
    const auto black_square = square.is_dark();
    const auto empty_square = piece.type() == chess_piece_type::NONE;
    const auto unfocused_text_font = !focused && !_using_full_cell;

    // On monochrome terminals, we can't have the board and pieces displayed in
    // different colors, so the way we handle white on white, and black on
    // black, is by using a hollow representation of the piece. When using a
    // color terminal, the pieces will always be solid though. And when focused,
    // the square will be rendered with a solid background, regardless of the
    // color, so it follows the same rule as if it were a white square. The
    // other special case is an empty square, which is a displayed as "hollow"
    // when it's black and unfocused on terminals with text-only fonts. We use
    // a different rendering technique on those devices, which requires a solid
    // glyph for the black square rather than a space.
    const auto hollow = [&]() {
        if (empty_square)
            return black_square && unfocused_text_font;
        if (_screen.using_colors())
            return false;
        if (focused)
            return white_piece;
        return white_piece == white_square;
    };

    // The _attrs array is preconfigured with either color_attrs or mono_attrs
    // (defined at the top of this file) based on the terminal type. So for
    // squares that aren't empty, the code below is obvious. For empty squares,
    // we typicaly use the board colors reversed for white, and the bright
    // attribute removed for a dark square, but devices with text-only fonts
    // are a special case. As explained above, they have a separate glyph for
    // the black square, so both squares are rendered with reverse attributes.
    const auto base_color = [&]() {
        if (empty_square) {
            if (white_square || unfocused_text_font)
                return _attrs[BOARD_COLOR] | sgr::reverse;
            else
                return _attrs[BOARD_COLOR] & ~sgr::bright;
        } else {
            if (white_piece)
                return white_square ? _attrs[WHITE_PIECE_ON_WHITE] : _attrs[WHITE_PIECE_ON_BLACK];
            else
                return white_square ? _attrs[BLACK_PIECE_ON_WHITE] : _attrs[BLACK_PIECE_ON_BLACK];
        }
    };

    // I'm losing interest with the comments, and this is difficult to explain,
    // but if you look at the attr definitions at the top of the file, it'll
    // hopefully make sense. When a square is focused, we want to make the
    // background "glow" in a different color (a different shade on monochrome
    // devices). This is achieved by adding and/or removing attributes from the
    // base color.
    auto color = base_color();
    if (focused) {
        color &= ~_attrs[FOCUS_REMOVES];
        color |= _attrs[FOCUS_ADDS];
        if (empty_square) color |= sgr::reverse;
    }
    return {color, hollow()};
}

chess_square board::_square_at(const coord pos) const
{
    if (_playing_white) {
        const auto rank = chess_rank{7 - pos.y};
        const auto file = chess_file{pos.x};
        return {rank, file};
    } else {
        const auto rank = chess_rank{pos.y};
        const auto file = chess_file{7 - pos.x};
        return {rank, file};
    }
}

coord board::_position_of(const chess_square square) const
{
    if (_playing_white) {
        const auto y = 7 - square.rank();
        const auto x = 0 + square.file();
        return {y, x};
    } else {
        const auto y = 0 + square.rank();
        const auto x = 7 - square.file();
        return {y, x};
    }
}
