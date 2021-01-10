all:example_threadpool

example_threadpool:Example.cpp ../../threadpool/ThreadPool.h
	g++ -std=c++11 -o $@ $^ -lpthread

run:
	time valgrind ./example_threadpool

clean:
	rm -rf example_threadpool