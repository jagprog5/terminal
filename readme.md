# Basic Terminal

This is a basic terminal. From the ground up: using linux sys calls and SDL2 rendering.

It supports utf8 encoding. To test, run `cat utf8.txt`.

The scroll wheel and a few ANSI escape codes are implemented (`clear` and fg/bg colors 256/8/rgb). As a test, write `cat fancy.txt`.

Backspace is implemented, but not for default launched shell (sh). bash works.

Dependencies:

```bash
apt install libsdl2-dev libsdl2-2.0-0 libsdl2-ttf-2.0-0 libsdl2-ttf-dev libfreetype6-dev libfreetype6 libfontconfig1 libfontconfig1-dev
```

To run:
```
make && LIBGL_ALWAYS_SOFTWARE=true ./a.out # export needed for WSL valgrind cleanliness
```

This project is work in progress, though it might stop here. Not for any particular reason, just need to work on other things.
