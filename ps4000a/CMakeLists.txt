cmake_minimum_required(VERSION 3.10)
project(ps4000a)

# Set standard C version
set(CMAKE_C_STANDARD 11)

# Add paths to the local and global shared includes
include_directories(
    /opt/picoscope/include/libps4000a
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
)

# Shared sources from the ps4000a/shared directory
set(LOCAL_SHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibBlockps4000a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibRapidBlockps4000a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibStreamingps4000a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibAWGps4000a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/Libps4000a.c
)

# Shared sources from the top-level picosdk-c-examples/shared directory
set(GLOBAL_SHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/PicoBuffers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/PicoFileFunctions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/PicoScaling.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/pbPlots.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/supportLib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/PicoPlotting.c
)

# Combine all shared sources
set(ALL_SHARED_SOURCES ${LOCAL_SHARED_SOURCES} ${GLOBAL_SHARED_SOURCES})

# Ensure the mathematical library (m) is linked for Linux/macOS
# 1. Point to the directory
link_directories(/opt/picoscope/lib)
# 2. Use the shorthand name
set(LIBS ps4000a m)

# Tell the GNU linker to ignore multiple definitions of the same function
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,muldefs")

# 1. ps4000aAWG Example
add_executable(ps4000aAWG ps4000aAWG/ps4000aAWG.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps4000aAWG ${LIBS})

# 2. ps4000aBlock Example
add_executable(ps4000aBlock ps4000aBlock/ps4000aBlock.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps4000aBlock ${LIBS})

# 3. ps4000aRapidBlock Example
add_executable(ps4000aRapidBlock ps4000aRapidBlock/ps4000aRapidBlock.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps4000aRapidBlock ${LIBS})

# 4. ps4000aStreaming Example
add_executable(ps4000aStreaming ps4000aStreaming/ps4000aStreaming.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps4000aStreaming ${LIBS})

# 5. ps4000aTemplate Example
add_executable(ps4000aTemplate ps4000aTemplate/ps4000aTemplate.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps4000aTemplate ${LIBS})
