set(target fluidx3d)

set(_source_files
    graphics.cpp
    info.cpp
    kernel.cpp
    lbm.cpp
    lodepng.cpp
    main.cpp
    setup.cpp
    shapes.cpp
)

add_executable(${target} ${_source_files})

target_link_libraries(${target} PRIVATE project_warnings project_options pthread OpenCL)
target_include_directories(${target} PRIVATE include OpenCL/include)
