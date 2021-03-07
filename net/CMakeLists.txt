set(epoll_SRC epoll/epoll_reactor.cpp)

add_executable(epoll_main ${epoll_SRC})
target_link_libraries(epoll_main pthread)

set(IoReactor_SRC epoll/IoReactors.cpp epoll/main.cpp)
add_executable(IoReactor ${IoReactor_SRC})
target_link_libraries(IoReactor pthread)