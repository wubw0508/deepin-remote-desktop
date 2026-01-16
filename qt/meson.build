qt_inc = include_directories('.')

qt_core_sources = files('core/drd_runtime.cpp')
qt_session_sources = files('session/drd_session.cpp')
qt_transport_sources = files('transport/drd_transport.cpp')
qt_security_sources = files('security/drd_security.cpp')
qt_system_sources = files('system/drd_system.cpp')

drd_qt_core = static_library('drd-qt-core',
                             qt_core_sources,
                             include_directories: qt_inc,
                             dependencies: [qt_dep])

drd_qt_session = static_library('drd-qt-session',
                                qt_session_sources,
                                include_directories: qt_inc,
                                dependencies: [qt_dep])

drd_qt_transport = static_library('drd-qt-transport',
                                  qt_transport_sources,
                                  include_directories: qt_inc,
                                  dependencies: [qt_dep])

drd_qt_security = static_library('drd-qt-security',
                                 qt_security_sources,
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
  drd_qt_system
]
