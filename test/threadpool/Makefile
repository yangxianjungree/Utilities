all:example_threadpool

example_threadpool:Example.cpp ../../threadpool/ThreadPool.h
	g++ -o $@ $^ -lpthread

run:
	time valgrind ./example_threadpool

clean:
	rm -rf example_threadpool