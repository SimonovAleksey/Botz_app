cmake_minimum_required(VERSION 3.16)

project(RobotControlApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network WebSockets)

set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
set(APP_ICON ${CMAKE_SOURCE_DIR}/AppIcon.icns)
set_source_files_properties(${APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

if(WIN32)
    set(WIN_RC ${CMAKE_SOURCE_DIR}/app.rc)
endif()

add_executable(RobotControlApp
    MACOSX_BUNDLE
    main.cpp
    MainWindow.cpp
    network/WebSocketClient.cpp
    controllers/RobotController.cpp
    resources.qrc
    ${APP_ICON}
    ${WIN_RC}
)

set_target_properties(RobotControlApp PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Robot Controller"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.robotcontrolapp.app"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_ICON_FILE AppIcon.icns
)

target_include_directories(RobotControlApp PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(RobotControlApp
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)