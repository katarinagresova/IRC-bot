all: isabot
isabot: isabot.cpp
		g++ -std=c++11 -o isabot -g  -pedantic isabot.cpp 
