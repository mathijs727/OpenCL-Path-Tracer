BRIEF INFO ON THE UU/IGAD TEMPLATE

Purpose:
The template has been designed to make it easy to start coding C++
using games and graphics. It intends to offer the programmer a
simple library with the main purpose of providing a 32-bit graphical
window with a linear frame buffer. Some basic additional functionality
is available, such as sprites, bitmap fonts, basic multi-threading,
and vector math support (though GLM).

How to use:
1. Copy the template folder (or extract the zip) to a fresh folder for
   your project. 
2. Open the .sln file with VS2013/VS2015 or VS2013/VS2015 Express.
3. Replace the example code in game.cpp with your own code.
You can go further by:
- Expanding the game class in game.h;
- Implementing some of the empty functions for mouse and keyboard
  handling;
- Exploring the code of the template in surface.cpp and template.cpp.
At some point, and depending on your requirements, you may want to
advance to a more full-fledged library, or you can expand the template
with OpenGL or SDL2 code.

Credits
Although the template is small and bare bones, it still uses a lot of
code gathered over the years:
- EasyCE's 5x5 bitmap font (primarily used for debugging);
- EasyCE's surface class (with lots of modifications);
- GLM for vector math and FreeImage for bitmap file loading;
- Nils Desle's JobManager for efficient multi-threading.
- This version of the template uses SDL2 for low-level window handling,
  hopefully improving on the compatibility of earlier versions.

Copyright
This code is completely free to use and distribute in any form.

Utrecht, 2016, UU/NHTV/IGAD, Jacco Bikker.
Report problems and suggestions to bikker.j@gmail.com .