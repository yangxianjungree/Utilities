all:example_threadpool

example_threadpool:Example.cpp ../../threadpool/ThreadPool.h
	g++ -o $@ $^ -lpthread

clean:
	rm -rf example_threadpool