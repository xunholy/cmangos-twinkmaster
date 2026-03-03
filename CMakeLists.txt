# CMaNGOS Twinkmaster Module
# Cloned to: src/modules/twinkmaster/

set(TWINKMASTER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TwinkmasterModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TwinkmasterModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TwinkmasterModuleConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TwinkmasterModuleConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/npc_twinkmaster.cpp
)

target_sources(game PRIVATE ${TWINKMASTER_SRCS})
target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/twinkmaster.conf.dist.in"
    "${CMAKE_BINARY_DIR}/etc/twinkmaster.conf.dist"
)

install(FILES
    "${CMAKE_BINARY_DIR}/etc/twinkmaster.conf.dist"
    DESTINATION etc
)

message(STATUS "  -> Twinkmaster module enabled")
