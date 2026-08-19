VT Chess
========

![Screenshot](screenshot.png)

A simple chess playing application for DEC VT terminals. Ideally you'd
have a VT320 or better, but it should still be usable on terminals in the
VT200 range - it just won't look that good.

The engine is very basic, but good enough for casual play. At some point
I'd like to support third-party engines, though, so you can compete at a
grand master level if you like that sort of challenge.


Controls
--------

Use the arrow keys and `Enter` to select the piece you want to move, then
press `Enter` again to indicate the destination position.

You can also make a move by typing the SAN notation for the move and then
pressing `Enter`.

To quit the game early, type `quit` or press `Ctrl+C`.

By default you'll be assigned a colour randomly, but if you prefer playing
a particular colour, you can start the app with `--white` or `--black`.

To see more options, use `--help`.


Download
--------

The latest binaries can be found on GitHub at the following url:

https://github.com/j4james/vtchess/releases/latest

For Linux download `vtchess`, and for Windows download `vtchess.exe`.


Build Instructions
------------------

If you want to build this yourself, you'll need [CMake] version 3.15 or later
and a C++ compiler supporting C++20 or later.

1. Download or clone the source:  
   `git clone https://github.com/j4james/vtchess.git`

2. Change into the build directory:  
   `cd vtchess/build`

3. Generate the build project:  
   `cmake -D CMAKE_BUILD_TYPE=Release ..`

4. Start the build:  
   `cmake --build . --config Release`

[CMake]: https://cmake.org/


Credits
-------

I'm using the [Disservin Chess Libary] for managing the board state,
and validating moves.

[Disservin Chess Libary]: https://disservin.github.io/chess-library/


License
-------

The VT Chess source code and binaries are released under the MIT License.
See the [LICENSE] file for full license details.

[LICENSE]: LICENSE.txt
