set(TARGET Storage)
project(${TARGET})

add_library(${TARGET} SHARED
    src/Storage.cpp
)

target_include_directories(${TARGET} PRIVATE "include")
target_link_libraries(${TARGET} PUBLIC UE4SS)