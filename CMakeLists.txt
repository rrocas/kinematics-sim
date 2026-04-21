cmake_minimum_required(VERSION 4.3.1)

project(kinematics)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

add_executable(kinematics 
    main.cpp
    qcustomplot.cpp
    )

target_link_libraries(kinematics PRIVATE Qt6::Widgets Qt6::PrintSupport)