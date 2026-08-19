// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#include "engine.h"

#include "board.h"
#include "capabilities.h"
#include "input.h"
#include "options.h"
#include "os.h"
#include "record.h"
#include "screen.h"

#include <chrono>
#include <limits>
#include <random>
#include <thread>

using namespace std::chrono_literals;

namespace {

    constexpr auto MAX_SCORE = 1000000;
    constexpr auto MIN_SCORE = -MAX_SCORE;

    int random_int(const int min, const int max)
    {
        static auto random_engine = std::default_random_engine(std::random_device{}());
        return std::uniform_int_distribution<>{min, max}(random_engine);
    }

    bool random_bool()
    {
        return random_int(0, 1);
    }

}  // namespace

engine::engine(screen& screen, record& record, const capabilities& caps, const options& options)
    : _screen{screen}, _record{record}, _caps{caps}, _options{options}
{
}

bool engine::run()
{
    const auto search_depth = _options.level;
    const auto human_playing_white = _options.playing_white.value_or(random_bool());
    const auto using_full_cell = _caps.has_full_cell_fonts;

    board board{_screen, using_full_cell, human_playing_white};
    input input{_screen, board};

    board.render_background();

    // If the computer is playing white, wait a second before letting it move.
    if (!human_playing_white) std::this_thread::sleep_for(1s);

    try {
        for (auto i = 0; !_game_over(board); i++) {
            const auto white_to_move = (i % 2 == 0);
            if (white_to_move == human_playing_white)
                _human_move(board, input);
            else
                _computer_move(board, search_depth);
        }
        input.press_any_key();
        return input.popup_dialog("Play again?\n\n  &Yes  &No") == 'Y';
    } catch (...) {
        // User abandoned the game.
        return false;
    }
}

bool engine::_game_over(const board& board)
{
    const auto game_state = board.game_state();
    if (game_state == chess_result::NONE)
        return false;
    else {
        const auto check_mate = game_state == chess_result::CHECKMATE;
        _record.game_over(check_mate ? "CHECK MATE!" : "STALE MATE!");
        return true;
    }
}

void engine::_human_move(board& board, input& input)
{
    auto move = input.get_move();
    if (move.typeOf() == chess_move::PROMOTION) {
        const auto promote = [&](const auto piece_type) {
            const auto from = move.from();
            const auto to = move.to();
            return chess_move::make<chess_move::PROMOTION>(from, to, piece_type);
        };
        switch (input.popup_dialog("&Queen\n&Rook\n&Bishop\n&Knight")) {
            case 'Q': move = promote(chess_piece_type::QUEEN); break;
            case 'R': move = promote(chess_piece_type::ROOK); break;
            case 'B': move = promote(chess_piece_type::BISHOP); break;
            case 'K': move = promote(chess_piece_type::KNIGHT); break;
        }
    }
    _record.add(board.describe_move(move));
    board.apply_move(move);
}

void engine::_computer_move(board& board, const int search_depth)
{
    auto best_move = chess_move{};
    auto best_score = MIN_SCORE;
    auto best_count = 0;

    for (const auto& move : board.legal_moves()) {
        const auto score = board.preview_move(move, [&]() {
            // When evaluating the opponents move, their worst score is the
            // negative of our best. This is used to abort the search early
            // once they've equaled the worst, on the assumption that the
            // caller isn't interested in anything better than that. This top
            // level is a special case, though, because we still accepts scores
            // equal to the best, and if the next level ended early on such a
            // score, we may be accepting an invalid match. Adding 1 to the
            // threshold prevents that from happening.
            const auto worst_score = -best_score + 1;
            return _evaluate_move(board, search_depth, MIN_SCORE, worst_score);
        });
        if (score > best_score) {
            best_score = score;
            best_move = move;
            best_count = 1;
        } else if (score == best_score) {
            // If we have multiple moves with the same score, we pick one
            // randomly with a simple form of reservoir sampling. This makes
            // the gameplay a little more interesting.
            best_count++;
            if (random_int(1, best_count) == 1)
                best_move = move;
        }
    }

    board.visualize_move(best_move, 500ms);
    std::this_thread::sleep_for(500ms);
    _record.add(board.describe_move(best_move));
    board.apply_move(best_move);
}

int engine::_evaluate_move(board& board, const int depth, int best_score_so_far, int worst_score_so_far)
{
    const auto moves = board.legal_moves();
    const auto game_state = board.game_state(moves);

    // If the current move has resulted in check-mate, we assign a score higher
    // than any other possible ranking, and with the depth added so an earlier
    // check-mate will score higher than a later one. Note that this must not be
    // MAX_SCORE, because it needs to be greater than MIN_SCORE when negated.
    if (game_state == chess_result::CHECKMATE)
        return 1000 + depth;

    // If the current move has resulted in a stale-mate, we assign it a zero
    // score, since it's better than potentially losing moves (with a negative
    // score), but worse than potentially winning moves (which are positive).
    if (game_state != chess_result::NONE)
        return 0;

    // If we reached our maximum search depth, then we assign the move
    // a score by counting the material on the board.
    if (depth == 1)
        return _count_material(board);

    // Otherwise we look ahead to see what the opponent might do by evaluating
    // all of their possible moves. Their best score is negated to determine
    // our score, since what's good for them is bad for us.
    auto best_score = MIN_SCORE;
    for (const auto& move : moves) {
        const auto score = board.preview_move(move, [&]() {
            // When evaluating the opponent move, their best score so far is the
            // negative of our worse score, and vice versa.
            return _evaluate_move(board, depth - 1, -worst_score_so_far, -best_score_so_far);
        });
        if (score > best_score) {
            best_score = score;
            if (best_score > best_score_so_far) {
                best_score_so_far = best_score;
                // Once our best score reaches or exceeds the worst score so
                // far, there's no point in looking any further. Our opponent
                // (in the level above) is looking for our worst, so if we're
                // certain this branch can't be worst, we return immediately.
                if (best_score >= worst_score_so_far) break;
            }
        }
    }
    return -best_score;
}

int engine::_count_material(const board& board)
{
    // We can get a rough estimate for how good our position is just by counting
    // the material each side still has on the board. The count_material method
    // calculates the difference in the count between the two sides, returning a
    // positive value if the active player has more of the specified type than
    // their opponent. Each count is also scaled to assign a greater weight to
    // the more valuable pieces. There are a lot of theories about which numbers
    // are best to use here, but I've just gone with the most common values.
    // See: https://en.wikipedia.org/wiki/Chess_piece_relative_value
    auto score = 0;
    score += board.count_material(chess_piece_type::PAWN) * 1;
    score += board.count_material(chess_piece_type::KNIGHT) * 3;
    score += board.count_material(chess_piece_type::BISHOP) * 3;
    score += board.count_material(chess_piece_type::ROOK) * 5;
    score += board.count_material(chess_piece_type::QUEEN) * 9;
    return score;
}
