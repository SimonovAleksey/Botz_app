cmake_minimum_required(VERSION 3.16)

project(RobotControlApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets WebSockets)

add_executable(RobotControlApp
    main.cpp
    MainWindow.cpp
    network/WebSocketClient.cpp
    controllers/RobotController.cpp
)

target_include_directories(RobotControlApp PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(RobotControlApp
    Qt6::Widgets
    Qt6::WebSockets
)