target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        EventHandler.cc
        EventHandler.h
        HealthAndArmingCheckReport.cc
        HealthAndArmingCheckReport.h
        logging.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

#===========================================================================#

CPMAddPackage(
    NAME libevents
    GITHUB_REPOSITORY mavlink/libevents
    # 固定到与当前 MAVLink 快照及 EventHandler 旧 API 兼容的最后一个版本(2024-10-17)
    # 不可用 main: 其后续版本使用了新版 MAVLink 的 32 位 target system 接口
    GIT_TAG 9ef591c447fe0386d698bf6fb9a6d27e43988ee4
    SOURCE_SUBDIR libs/cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE libevents)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${libevents_SOURCE_DIR}/libs/cpp)
