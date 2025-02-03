cmake_minimum_required(VERSION 3.1.0)

project(poker_solver_ui)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

add_executable(poker_solver_ui
    main.cpp
    MainWindow.cpp
    RangeSelector.cpp
    StrategyExplorer.cpp  # <-- Ensure this file exists in your source tree.
    resources.qrc
)

target_link_libraries(poker_solver_ui PRIVATE
    Qt6::Widgets
)