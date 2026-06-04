if(QAccessibilityClient6_FOUND)
    set(HAVE_QACCESSIBILITYCLIENT TRUE)
else()
    set(HAVE_QACCESSIBILITYCLIENT FALSE)
endif()

if(LayerShellQt_FOUND)
    set(HAVE_LAYERSHELLQT TRUE)
else()
    set(HAVE_LAYERSHELLQT FALSE)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/focustrackconfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/focustrackconfig.h"
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/dockconfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dockconfig.h"
)

if(KMAG_WITH_PIPEWIRE)
    set(KMAG_WITH_PIPEWIRE TRUE)
else()
    set(KMAG_WITH_PIPEWIRE FALSE)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/screencapture/screencaptureconfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/screencaptureconfig.h"
)

ecm_setup_version(${RELEASE_SERVICE_VERSION}
    VARIABLE_PREFIX KMAG
    VERSION_HEADER kmag_version.h
)

add_executable(kmag)

# C++20: the PipeWire backend uses designated initializers for pw_stream_events.
set_target_properties(kmag PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

target_sources(kmag PRIVATE
    kmagzoomview.cpp kmagzoomview.h
    kmagselrect.cpp kmagselrect.h
    kmag.cpp kmag.h
    colorsim.cpp colorsim.h
    screencapture/screencapturemanager.cpp screencapture/screencapturemanager.h
    screencapture/framecrop.cpp screencapture/framecrop.h
    screencapture/x11capturemanager.cpp screencapture/x11capturemanager.h
    docking/dockmath.cpp docking/dockmath.h
    docking/dockingmanager.cpp docking/dockingmanager.h
    docking/x11dockingmanager.cpp docking/x11dockingmanager.h
    docking/dockcontrolpanel.cpp docking/dockcontrolpanel.h
    docking/dockcontroller.cpp docking/dockcontroller.h
    main.cpp

    kmag.qrc
)

target_include_directories(kmag PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/screencapture)

target_link_libraries(kmag
    KF6::KIOCore
    KF6::I18n
    KF6::WindowSystem
    KF6::XmlGui
    KF6::Crash
    KF6::ConfigCore   # KSharedConfig/KConfigGroup (restore-token persistence)
    Qt6::Core
    Qt6::Widgets
    Qt6::PrintSupport
)

if(KMAG_WITH_PIPEWIRE)
    target_sources(kmag PRIVATE
        screencapture/pipewirecapturemanager.cpp
        screencapture/pipewirecapturemanager.h
    )
    # QtDBus drives the XDG ScreenCast portal handshake (PipeWire backend only).
    target_link_libraries(kmag Qt6::DBus PkgConfig::PipeWire PkgConfig::Spa)
endif()

if(QAccessibilityClient6_FOUND)
    target_link_libraries(kmag QAccessibilityClient6)
endif()

if(LayerShellQt_FOUND)
    target_sources(kmag PRIVATE docking/waylanddockingmanager.cpp docking/waylanddockingmanager.h)
    target_compile_definitions(kmag PRIVATE HAVE_LAYERSHELLQT)
    target_link_libraries(kmag LayerShellQt::Interface)
endif()

install(TARGETS kmag ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(PROGRAMS org.kde.kmag.desktop  DESTINATION  ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kmag.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
