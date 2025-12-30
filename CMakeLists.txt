cmake_minimum_required(VERSION 3.16)
project(whatsit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# REQUIRED for Q_OBJECT
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS
    Widgets
    WebEngineWidgets
    WebEngineCore
)

add_executable(whatsit
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)

target_link_libraries(whatsit PRIVATE
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
)

install(TARGETS whatsit RUNTIME DESTINATION bin)
install(FILES data/whatsit.desktop DESTINATION share/applications)
install(FILES resources/whatsit.png DESTINATION share/icons/hicolor/256x256/apps)
