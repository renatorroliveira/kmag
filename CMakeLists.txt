cmake_minimum_required (VERSION 3.16)

# KDE Application Version, managed by release script
set (RELEASE_SERVICE_VERSION_MAJOR "26")
set (RELEASE_SERVICE_VERSION_MINOR "07")
set (RELEASE_SERVICE_VERSION_MICRO "70")
set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

project(kmag VERSION ${RELEASE_SERVICE_VERSION})

set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.11.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMDeprecationSettings)

include(ECMInstallIcons)
include(FeatureSummary)
include(ECMSetupVersion)

option(BUILD_DOC "Whether to build the documentation" ON)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Widgets
    PrintSupport
    DBus
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    KIO
    XmlGui
    Crash
    WindowSystem
    Config
)

if (BUILD_DOC)
    find_package(KF6DocTools ${KF_MIN_VERSION})
    set_package_properties(KF6DocTools PROPERTIES
        DESCRIPTION "Tools to generate documentation"
        TYPE REQUIRED
)
endif()

find_package(QAccessibilityClient6 0.4 CONFIG)
set_package_properties(QAccessibilityClient6 PROPERTIES
    DESCRIPTION "KDE client-side accessibility library"
    URL "https://commits.kde.org/libqaccessibilityclient"
    TYPE OPTIONAL
    PURPOSE "Required to enable keyboard focus tracking."
)

find_package(LayerShellQt 6.5.5 CONFIG)
set_package_properties(LayerShellQt PROPERTIES
    DESCRIPTION "Qt component for the Wayland wlr-layer-shell protocol"
    URL "https://invent.kde.org/plasma/layer-shell-qt"
    TYPE OPTIONAL
    PURPOSE "Required for screen-edge docking (docked mode) on Wayland."
)

option(KMAG_WITH_PIPEWIRE "Build the Wayland PipeWire/portal screen-capture backend" ON)

if(KMAG_WITH_PIPEWIRE)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(PipeWire IMPORTED_TARGET libpipewire-0.3>=1.0.0)
        pkg_check_modules(Spa IMPORTED_TARGET libspa-0.2>=0.2)
    endif()
    if(NOT PkgConfig_FOUND OR NOT PipeWire_FOUND OR NOT Spa_FOUND)
        message(STATUS "PipeWire/SPA not found; disabling KMAG_WITH_PIPEWIRE")
        set(KMAG_WITH_PIPEWIRE OFF)
    endif()
endif()

add_feature_info(KMAG_WITH_PIPEWIRE KMAG_WITH_PIPEWIRE
    "Wayland screen capture via XDG ScreenCast portal + PipeWire")

ecm_set_disabled_deprecation_versions(QT 6.9.0  KF 6.18.0)

add_subdirectory( pics )
add_subdirectory( src )

if (BUILD_DOC)
    add_subdirectory(doc)
    kdoctools_install(po)
endif()

ki18n_install(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
