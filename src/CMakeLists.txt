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
    SDL_FontCache/SDL_FontCache.c
)

add_executable(${target} ${_source_files})

find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

target_link_libraries(${target} PRIVATE project_warnings project_options)

target_include_directories(${target} PRIVATE
    include
    OpenCL/include
    SDL_FontCache/include
    ${ZLIB_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_ttf_INCLUDE_DIRS}
)

target_link_libraries(${target} PRIVATE
    pthread
    OpenCL
    z
    SDL2
    SDL2_ttf
)