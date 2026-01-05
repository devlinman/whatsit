cmake_minimum_required(VERSION 3.16)
project(whatsit LANGUAGES CXX)

# -----------------------------
# Compiler / Qt setup
# -----------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# -----------------------------
# Dependencies
# -----------------------------
find_package(Vulkan REQUIRED)

# -----------------------------
# Qt6
# -----------------------------
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Widgets
    WebEngineWidgets
    WebEngineCore
)

# -----------------------------
# KDE / ECM / KF6
# -----------------------------
find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(KF6Config REQUIRED)
find_package(KF6Notifications REQUIRED)
find_package(KF6StatusNotifierItem REQUIRED)
find_package(KF6WidgetsAddons REQUIRED)
find_package(KF6IconThemes REQUIRED)

# -----------------------------
# Sources
# -----------------------------
set(WHATSIT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/configmanager.cpp
    src/webenginehelper.cpp
    src/traymanager.cpp
    src/ipcmanager.cpp
    src/logger.cpp
)

set(WHATSIT_HEADERS
    src/mainwindow.h
    src/configmanager.h
    src/webenginehelper.h
    src/traymanager.h
    src/ipcmanager.h
    src/logger.h
)

add_executable(whatsit
    ${WHATSIT_SOURCES}
    ${WHATSIT_HEADERS}
)

# Arch / KF6 include layout
target_include_directories(whatsit PRIVATE /usr/include/KF6)

# -----------------------------
# Linking
# -----------------------------
target_link_libraries(whatsit PRIVATE
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore

    KF6::ConfigCore
    KF6::Notifications
    KF6::StatusNotifierItem
    KF6::WidgetsAddons
    KF6::IconThemes
    KF6::IconWidgets
)

# -----------------------------
# Installation
# -----------------------------
install(TARGETS whatsit
    RUNTIME DESTINATION bin
)

install(
    FILES data/whatsit.desktop
    DESTINATION share/applications
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

install(
    FILES data/whatsit.notifyrc
    DESTINATION share/knotifications6
)


install(FILES resources/whatsit.png
    DESTINATION share/icons/hicolor/256x256/apps
)

install(FILES resources/whatsit_mono.png
    DESTINATION share/icons/hicolor/256x256/apps
)

install(FILES resources/WhatsApp.png
    DESTINATION share/icons/hicolor/256x256/apps
)

# -----------------------------
# Uninstall support
# -----------------------------
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE
        @ONLY
    )

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P
                ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()
