# CMakeList.txt: файл проекта CMake верхнего уровня;

cmake_minimum_required (VERSION 3.8)

project ("CMake_ProcMon")

add_definitions(-DUNICODE -D_UNICODE -D_WINDOWS)

add_subdirectory ("UtilsProc_LIB")
add_subdirectory ("ProcMon4")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
