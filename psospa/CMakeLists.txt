cmake_minimum_required(VERSION 3.10)
project(psospa)

# Set standard C version
set(CMAKE_C_STANDARD 11)

# Add paths to the local and global shared includes
include_directories(
    /opt/picoscope/include/libpsospa
    /opt/picoscope/include/libps6000a
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
)

# Shared sources from the psospa/shared directory
set(LOCAL_SHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibBlockpsospa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibRapidBlockpsospa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibStreamingpsospa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/LibAWGpsospa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/Libpsospa.c
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
set(LIBS psospa m)

# Tell the GNU linker to ignore multiple definitions of the same function
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,muldefs")

# 1. psospaAWG Example
add_executable(psospaAWG psospaAWG/psospaAWG.c ${ALL_SHARED_SOURCES})
target_link_libraries(psospaAWG ${LIBS})

# 2. psospaBlock Example
add_executable(psospaBlock psospaBlock/psospaBlock.c ${ALL_SHARED_SOURCES})
target_link_libraries(psospaBlock ${LIBS})

# 3. psospaRapidBlock Example
add_executable(psospaRapidBlock psospaRapidBlock/psospaRapidBlock.c ${ALL_SHARED_SOURCES})
target_link_libraries(psospaRapidBlock ${LIBS})

# 4. psospaStreaming Example
add_executable(psospaStreaming psospaStreaming/psospaStreaming.c ${ALL_SHARED_SOURCES})
target_link_libraries(psospaStreaming ${LIBS})

# 5. psospaTemplate Example
add_executable(psospaTemplate psospaTemplate/psospaTemplate.c ${ALL_SHARED_SOURCES})
target_link_libraries(psospaTemplate ${LIBS})
