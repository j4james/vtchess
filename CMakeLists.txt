cmake_minimum_required(VERSION 3.15)
project(vtchess)

set(
    MAIN_FILES
    "src/main.cpp"
    "src/board.cpp"
    "src/capabilities.cpp"
    "src/coloring.cpp"
    "src/engine.cpp"
    "src/font.cpp"
    "src/input.cpp"
    "src/options.cpp"
    "src/os.cpp"
    "src/record.cpp"
    "src/screen.cpp"
)

set(
    DOC_FILES
    "README.md"
    "LICENSE.txt"
)

add_executable(vtchess ${MAIN_FILES})

set_target_properties(vtchess PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED On)
source_group("Doc Files" FILES ${DOC_FILES})
