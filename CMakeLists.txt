set(TARGET Storage)
project(${TARGET})

add_library(${TARGET} SHARED
    src/dllmain.cpp
)

target_link_libraries(${TARGET} PUBLIC EvrimaCommon)
target_link_libraries(${TARGET} PUBLIC UE4SS)