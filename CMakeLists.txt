cmake_minimum_required(VERSION 3.29)
project(hexspanned)

set(CMAKE_CXX_STANDARD 20)

add_executable(hexspanned main.cpp
        imfilebrowser.h)

find_package(GLFW3 CONFIG REQUIRED)
target_link_libraries(hexspanned PRIVATE glfw)

find_package(glad CONFIG REQUIRED)
target_link_libraries(hexspanned PRIVATE glad::glad)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(hexspanned PRIVATE imgui::imgui)

find_package(glm CONFIG REQUIRED)
target_link_libraries(hexspanned PRIVATE glm::glm)