cmake_minimum_required(VERSION 3.16)
project(pinwheel_scheduling VERSION 1.0.0 LANGUAGES CXX)

find_package(Boost CONFIG REQUIRED COMPONENTS program_options)

foreach(program IN ITEMS packing_schedule_find packing_schedule_quality)
    add_executable(${program} ${program}.cpp)
    target_compile_features(${program} PRIVATE cxx_std_20)
    set_target_properties(${program} PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(${program} PRIVATE Boost::program_options)
endforeach()
