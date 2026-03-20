cmake_minimum_required(VERSION 3.16)
project(lg-reminder VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(lg-reminder WIN32 lg-reminder.cpp)

# 设置输出目录变量
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lg-reminder)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/o)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/o)

# 添加源文件
target_sources(lg-reminder PRIVATE
    res/lg-reminder.cpp
    res/lg-reminder.rc
)

# 设置属性
set_target_properties(lg-reminder PROPERTIES
    RUNTIME_OUTPUT_NAME "lg-reminder"
)

# Windows特定设置
if(WIN32)
    # 启用大地址支持
    set_target_properties(lg-reminder PROPERTIES
        LINK_FLAGS "/LARGEADDRESSAWARE"
    )
endif()

# 安装规则
install(TARGETS lg-reminder RUNTIME DESTINATION bin)
install(FILES res/icon.ico DESTINATION share/icons)
