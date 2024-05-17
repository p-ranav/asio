all:
	g++ -std=c++17 -Iasio/include -O3 -o httpserver httpserver.cpp
clean:
	rm -f httpserver