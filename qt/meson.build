qt_inc = include_directories('.')

# 生成 Qt 元对象代码
moc_sources = []

# 使用 Qt 6 版本的 moc 工具
moc_cmd = '/usr/lib/qt6/libexec/moc'

# DrdQtRdpSession 的 MOC 处理
drd_rdp_session_moc = custom_target('drd_rdp_session_moc',
                                   input: 'session/drd_rdp_session.h',
                                   output: 'drd_rdp_session.moc',
                                   command: [moc_cmd, '-o', '@OUTPUT@', '@INPUT@'])
moc_sources += drd_rdp_session_moc

# DrdQtRdpGraphicsPipeline 的 MOC 处理
drd_rdp_graphics_pipeline_moc = custom_target('drd_rdp_graphics_pipeline_moc',
                                              input: 'session/drd_rdp_graphics_pipeline.h',
                                              output: 'drd_rdp_graphics_pipeline.moc',
                                              command: [moc_cmd, '-o', '@OUTPUT@', '@INPUT@'])
moc_sources += drd_rdp_graphics_pipeline_moc

# DrdQtRdpListener 的 MOC 处理 - 定义在 drd_transport.h 中
drd_rdp_listener_moc = custom_target('drd_rdp_listener_moc',
                                     input: 'transport/drd_transport.h',
                                     output: 'drd_rdp_listener.moc',
                                     command: [moc_cmd, '-o', '@OUTPUT@', '@INPUT@'])
moc_sources += drd_rdp_listener_moc

# DrdQtTlsCredentials 的 MOC 处理
drd_tls_credentials_moc = custom_target('drd_tls_credentials_moc',
                                         input: 'security/drd_tls_credentials.h',
                                         output: 'drd_tls_credentials.moc',
                                         command: [moc_cmd, '-o', '@OUTPUT@', '@INPUT@'])
moc_sources += drd_tls_credentials_moc

# DrdQtServerRuntime 的 MOC 处理
drd_server_runtime_moc = custom_target('drd_server_runtime_moc',
                                       input: 'core/drd_server_runtime.h',
                                       output: 'drd_server_runtime.moc',
                                       command: [moc_cmd, '-o', '@OUTPUT@', '@INPUT@'])
moc_sources += drd_server_runtime_moc

qt_core_sources = files(
  'core/drd_runtime.cpp',
  'core/drd_application.cpp',
  'core/drd_config.cpp',
  'core/drd_server_runtime.cpp',
  'core/drd_encoding_options.cpp',
  'core/drd_dbus_constants.cpp'
) + [drd_server_runtime_moc]
qt_session_sources = files(
  'session/drd_rdp_session.cpp',
  'session/drd_rdp_graphics_pipeline.cpp'
) + [drd_rdp_session_moc, drd_rdp_graphics_pipeline_moc]

qt_transport_sources = files('transport/drd_transport.cpp') + [drd_rdp_listener_moc]
qt_security_sources = files(
  'security/drd_security.cpp',
  'security/drd_tls_credentials.cpp',
  'security/drd_local_session.cpp',
  'security/drd_nla_sam.cpp'
) + [drd_tls_credentials_moc]
qt_capture_sources = files(
  'capture/drd_capture_manager.cpp',
  'capture/drd_x11_capture.cpp'
)
qt_encoding_sources = files('encoding/drd_encoding_manager.cpp')
qt_input_sources = files(
  'input/drd_input_dispatcher.cpp',
  'input/drd_x11_input.cpp'
)
qt_system_sources = files(
  'system/drd_system_daemon.cpp',
  'system/drd_handover_daemon.cpp'
)

drd_qt_core = static_library('drd-qt-core',
                             qt_core_sources,
                             include_directories: qt_inc,
                             dependencies: [qt_dep])

drd_qt_session = static_library('drd-qt-session',
                                 qt_session_sources,
                                 include_directories: qt_inc,
                                 dependencies: [qt_dep, freerdp_server_dep, freerdp_core_dep, winpr_dep])

drd_qt_transport = static_library('drd-qt-transport',
                                   qt_transport_sources,
                                   include_directories: qt_inc,
                                   dependencies: [qt_dep, freerdp_server_dep, freerdp_core_dep, winpr_dep])

drd_qt_security = static_library('drd-qt-security',
                                   qt_security_sources,
                                   include_directories: qt_inc,
                                   dependencies: [qt_dep, freerdp_server_dep, freerdp_core_dep, winpr_dep])

drd_qt_capture = static_library('drd-qt-capture',
                                 qt_capture_sources,
                                 include_directories: qt_inc,
                                 dependencies: [qt_dep])

drd_qt_encoding = static_library('drd-qt-encoding',
                                  qt_encoding_sources,
                                  include_directories: qt_inc,
                                  dependencies: [qt_dep])

drd_qt_input = static_library('drd-qt-input',
                               qt_input_sources,
                               include_directories: qt_inc,
                               dependencies: [qt_dep])

drd_qt_system = static_library('drd-qt-system',
                                qt_system_sources,
                                include_directories: qt_inc,
                                dependencies: [qt_dep])

drd_qt_includes = qt_inc

drd_qt_libs = [
  drd_qt_core,
  drd_qt_session,
  drd_qt_transport,
  drd_qt_security,
  drd_qt_capture,
  drd_qt_encoding,
  drd_qt_input,
  drd_qt_system
]

# 可执行文件
drd_qt_exe = executable('deepin-remote-desktop',
                        files('main.cpp'),
                        include_directories: qt_inc,
                        dependencies: [qt_dep, freerdp_server_dep, freerdp_core_dep, winpr_dep,
                                       glib_dep, gio_dep, gio_unix_dep, gobject_dep,
                                       x11_dep, xext_dep, xdamage_dep, xfixes_dep, xtst_dep,
                                       pam_dep, avcodec_dep, avutil_dep, swscale_dep, vaapi_dep],
                        link_with: drd_qt_libs,
                        install: true,
                        install_dir: get_option('bindir'))
