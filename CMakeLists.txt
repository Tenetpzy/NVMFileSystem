cmake_minimum_required(VERSION 3.10)

project(NVMFileSystem)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
#set(CMAKE_C_STANDARD 11)
#set(CMAKE_C_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)
if(CMAKE_USE_PTHREADS_INIT)
    #list(APPEND CMAKE_C_FLAGS "-pthread")
else()
    message (FATAL_ERROR "Cannot find pthread")
endif()


FILE(GLOB Sources "src/*.c")
FILE(GLOB Headers "include/*.h")

add_executable(NVMFileSystem ${Sources} ${Headers})

target_include_directories(NVMFileSystem PRIVATE include)
target_link_libraries(NVMFileSystem PRIVATE Threads::Threads)