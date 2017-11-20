all: isabot
isabot: isabot.cpp
		g++ -std=c++11 -o isabot -Werror -Wall -pedantic isabot.cpp
