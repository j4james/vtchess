// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#include "options.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>

options::options(const int argc, const char* argv[])
{
    for (auto i = 1; i < argc && !exit; i++) {
        const auto arg = std::string{argv[i]};
        if (arg == "--white") {
            playing_white = true;
        } else if (arg == "--black") {
            playing_white = false;
        } else if (arg == "--mono") {
            color = false;
        } else if (arg == "--yolo") {
            yolo = true;
        } else if (arg == "--level" && i + 1 < argc) {
            try {
                level = std::stoi(argv[++i]);
                level = std::clamp(level, 1, 8);
            } catch (std::exception) {
                // ignore invalid level
            }
        } else if (arg == "--help") {
            std::cout << "Usage: vtchess [OPTION]...\n\n";
            std::cout << "  --white       play as white\n";
            std::cout << "  --black       play as black\n";
            std::cout << "  --level N     set difficulty (1 to 8)\n";
            std::cout << "  --mono        monochrome display\n";
            std::cout << "  --yolo        bypass compatibility checks\n";
            std::cout << "  --help        display this help and exit\n";
            exit = true;
        } else {
            std::cout << "VT Chess: unrecognized option '" << arg << "'\n";
            std::cout << "Try 'vtchess --help' for more information.\n";
            exit = true;
        }
    }
}
