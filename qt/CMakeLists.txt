cmake_minimum_required(VERSION 3.16)
project(deepin-remote-desktop VERSION 1.0.0 LANGUAGES C CXX)

# 启用 Qt 自动 MOC、UIC 和 RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 调试配置
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type: Debug or Release")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build enabled - 启用调试模式")
    add_compile_options(-g -O0 -ggdb -fno-omit-frame-pointer)
    add_link_options(-g)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
else()
    message(STATUS "Release build enabled - 启用发布模式")
    add_compile_options(-O2)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Network)

# 查找FreeRDP
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREERDP REQUIRED freerdp3)
pkg_check_modules(FREERDP_SERVER REQUIRED freerdp-server3)
pkg_check_modules(WINPR REQUIRED winpr3)

# 查找GLib
pkg_check_modules(GLIB REQUIRED glib-2.0 gobject-2.0 gio-2.0)

# 查找X11
find_package(X11 REQUIRED)
find_package(X11 REQUIRED COMPONENTS Xext)

# 查找X11扩展库
pkg_check_modules(XDAMAGE REQUIRED xdamage)
pkg_check_modules(XFIXES REQUIRED xfixes)

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/qt
    ${FREERDP_INCLUDE_DIRS}
    ${FREERDP_SERVER_INCLUDE_DIRS}
    ${WINPR_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${XDAMAGE_INCLUDE_DIRS}
    ${XFIXES_INCLUDE_DIRS}
)

# 链接目录
link_directories(
    ${FREERDP_LIBRARY_DIRS}
    ${FREERDP_SERVER_LIBRARY_DIRS}
    ${WINPR_LIBRARY_DIRS}
    ${GLIB_LIBRARY_DIRS}
)

# 创建主可执行文件
add_executable(deepin-remote-desktop
    main.cpp
    core/drd_application.cpp
    core/drd_config.cpp
    core/drd_server_runtime.cpp
    security/drd_tls_credentials.cpp
    session/drd_rdp_session.cpp
    session/drd_rdp_graphics_pipeline.cpp
    transport/drd_rdp_listener.cpp
    system/drd_system_daemon.cpp
    system/drd_handover_daemon.cpp
    utils/drd_frame.cpp
    utils/drd_frame_queue.cpp
    capture/drd_capture_manager.cpp
    capture/drd_x11_capture.cpp
    encoding/drd_encoding_manager.cpp
)

# 链接库
target_link_libraries(deepin-remote-desktop
    Qt6::Core
    Qt6::Network
    ${FREERDP_LIBRARIES}
    ${FREERDP_SERVER_LIBRARIES}
    ${WINPR_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${X11_LIBRARIES}
    ${X11_Xext_LIB}
    ${XDAMAGE_LIBRARIES}
    ${XFIXES_LIBRARIES}
)

# 设置编译选项
target_compile_options(deepin-remote-desktop PRIVATE
    ${FREERDP_CFLAGS_OTHER}
    ${FREERDP_SERVER_CFLAGS_OTHER}
    ${WINPR_CFLAGS_OTHER}
    ${GLIB_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# 确保调试信息生成
set_target_properties(deepin-remote-desktop PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_FLAGS "-g"
)

# 打印构建信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler flags: ${CMAKE_CXX_FLAGS}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug symbols enabled")
    message(STATUS "Optimization level: O0")
else()
    message(STATUS "Debug symbols disabled")
    message(STATUS "Optimization level: O2")
endif()

# 安装规则
install(TARGETS deepin-remote-desktop
    RUNTIME DESTINATION bin
)

# 安装systemd服务文件
install(FILES data/deepin-remote-desktop-system.service
    DESTINATION lib/systemd/system
)
install(FILES data/deepin-remote-desktop-user.service
    DESTINATION lib/systemd/user
)
install(FILES data/deepin-remote-desktop-handover.service
    DESTINATION lib/systemd/user
)

# 安装配置文件
install(DIRECTORY data/config.d/
    DESTINATION share/deepin-remote-desktop/config.d
)

# 安装证书文件
install(DIRECTORY data/certs/
    DESTINATION share/deepin-remote-desktop/certs
)

# 安装DBus配置文件
install(FILES data/org.deepin.RemoteDesktop.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d
)

# 安装greeter配置文件
install(FILES data/11-deepin-remote-desktop-handover
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/deepin/greeters.d
)

# 打印配置信息
message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "FreeRDP libraries: ${FREERDP_LIBRARIES}")
message(STATUS "WinPR libraries: ${WINPR_LIBRARIES}")
message(STATUS "GLib libraries: ${GLIB_LIBRARIES}")
