cmake_minimum_required(VERSION 3.10)
project(ps6000a)

# Set standard C version
set(CMAKE_C_STANDARD 11)

# Add paths to the local and global shared includes
include_directories(
    /opt/picoscope/include/libps6000a
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
)

# Shared sources from the ps6000a/shared directory
set(LOCAL_SHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibBlockps6000a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibRapidBlockps6000a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibStreamingps6000a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibAWGps6000a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/Libps6000a.c
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
set(LIBS ps6000a m)

# Tell the GNU linker to ignore multiple definitions of the same function
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,muldefs")

# 1. ps6000aAWG Example
add_executable(ps6000aAWG ps6000aAWG/ps6000aAWG.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps6000aAWG ${LIBS})

# 2. ps6000aBlock Example
add_executable(ps6000aBlock ps6000aBlock/ps6000aBlock.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps6000aBlock ${LIBS})

# 3. ps6000aRapidBlock Example
add_executable(ps6000aRapidBlock ps6000aRapidBlock/ps6000aRapidBlock.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps6000aRapidBlock ${LIBS})

# 4. ps6000aStreaming Example
add_executable(ps6000aStreaming ps6000aStreaming/ps6000aStreaming.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps6000aStreaming ${LIBS})

# 5. ps6000aTemplate Example
add_executable(ps6000aTemplate ps6000aTemplate/ps6000aTemplate.c ${ALL_SHARED_SOURCES})
target_link_libraries(ps6000aTemplate ${LIBS})
