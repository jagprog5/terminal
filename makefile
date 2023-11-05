all:
	g++ -O1 --std=c++17 main.cpp \
	-lpthread \
	-lfontconfig \
	$$(pkg-config --cflags --libs sdl2 SDL2_ttf)
