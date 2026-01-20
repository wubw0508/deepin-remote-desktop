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

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Network)

# 查找FreeRDP
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREERDP REQUIRED freerdp3)
pkg_check_modules(WINPR REQUIRED winpr3)

# 查找GLib
pkg_check_modules(GLIB REQUIRED glib-2.0 gobject-2.0 gio-2.0)

# 查找X11
find_package(X11 REQUIRED)

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/qt
    ${FREERDP_INCLUDE_DIRS}
    ${WINPR_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
)

# 链接目录
link_directories(
    ${FREERDP_LIBRARY_DIRS}
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
    transport/drd_rdp_listener.cpp
    system/drd_system_daemon.cpp
    system/drd_handover_daemon.cpp
    utils/drd_log.cpp
)

# 链接库
target_link_libraries(deepin-remote-desktop
    Qt6::Core
    Qt6::Network
    ${FREERDP_LIBRARIES}
    ${WINPR_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${X11_LIBRARIES}
)

# 设置编译选项
target_compile_options(deepin-remote-desktop PRIVATE
    ${FREERDP_CFLAGS_OTHER}
    ${WINPR_CFLAGS_OTHER}
    ${GLIB_CFLAGS_OTHER}
    -Wall
    -Wextra
)

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