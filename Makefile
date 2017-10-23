all: isabot
isabot: isabot.cpp
		g++ -std=c++11 -o isabot -g -Werror -Wall -pedantic isabot.cpp
server: echo-udp-server.c
		g++ -std=c++11 -o server -g -pedantic echo-udp-server.c
old: main.cpp
		g++ -std=c++11 -o old -g  -pedantic main.cpp
