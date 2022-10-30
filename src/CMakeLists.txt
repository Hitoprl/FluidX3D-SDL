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
    opencl.cpp
    sdl_graphics.cpp
)

add_executable(${target} ${_source_files})

find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)

target_link_libraries(${target} PRIVATE project_warnings project_options)

target_include_directories(${target} PRIVATE
    include
    OpenCL/include
    ${ZLIB_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(${target} PRIVATE
    pthread
    OpenCL
    ${ZLIB_LIBRARIES}
    ${SDL2_LIBRARIES}
)