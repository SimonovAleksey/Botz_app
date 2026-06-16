cmake_minimum_required(VERSION 3.16)

project(RobotControlApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network WebSockets)

add_executable(RobotControlApp
    MACOSX_BUNDLE
    main.cpp
    MainWindow.cpp
    network/WebSocketClient.cpp
    controllers/RobotController.cpp
    resources.qrc
)

set_target_properties(RobotControlApp PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Robot Controller"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.robotcontrolapp.app"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
)

target_include_directories(RobotControlApp PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(RobotControlApp
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)