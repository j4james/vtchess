// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#pragma once

#include <optional>

class options {
public:
    options(const int argc, const char* argv[]);

    std::optional<bool> playing_white;
    int level = 4;
    bool color = true;
    bool yolo = false;
    bool exit = false;
};
