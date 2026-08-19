// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#pragma once

class board;
class capabilities;
class input;
class options;
class record;
class screen;

class engine {
public:
    engine(screen& screen, record& record, const capabilities& caps, const options& options);
    bool run();

private:
    bool _game_over(const board& board);
    void _human_move(board& board, input& input);
    void _computer_move(board& board, const int search_depth);
    static int _evaluate_move(board& board, const int depth, int alpha, int beta);
    static int _count_material(const board& board);

    screen& _screen;
    record& _record;
    const capabilities& _caps;
    const options& _options;
};
