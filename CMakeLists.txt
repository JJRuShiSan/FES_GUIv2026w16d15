cmake_minimum_required(VERSION 3.16)
project(FES_Thesis_GUI LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    main.cpp
    startwindow.cpp
    mainwindow.cpp
    electrodewindow.cpp
    sessionwindow.cpp
    historywindow.cpp
    spihandler.cpp
    gpioexpander.cpp
)

set(HEADERS
    startwindow.h
    mainwindow.h
    electrodewindow.h
    sessionwindow.h
    historywindow.h
    spihandler.h
    gpioexpander.h
)

set(RESOURCES
    resources/logo.qrc
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

find_library(PIGPIO_LIB pigpio REQUIRED)
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${PIGPIO_LIB}
)
