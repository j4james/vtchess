// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#pragma once

#include <string>

class capabilities;
class options;

class coloring {
public:
    coloring(capabilities& caps, const options& options);
    ~coloring();

private:
    bool _using_colors;
    std::string _color_assignment;
    std::string _color_table;
};
