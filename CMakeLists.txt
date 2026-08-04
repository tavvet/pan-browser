cmake_minimum_required(VERSION 3.24)

project(PanBrowserQt VERSION 0.2.0 LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(APPLE AND EXISTS "/opt/homebrew/opt/qtbase")
    set(QT_DISABLE_NO_DEFAULT_PATH_IN_QT_PACKAGES ON)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew/opt/qtbase"
        "/opt/homebrew/opt/qtdeclarative"
        "/opt/homebrew/opt/qtpositioning"
        "/opt/homebrew/opt/qtsvg"
        "/opt/homebrew/opt/qtwebchannel"
        "/opt/homebrew/opt/qtwebengine"
    )
endif()

find_package(Qt6 6.10 REQUIRED COMPONENTS
    Core
    Network
    Svg
    Test
    Widgets
)
find_package(Qt6WebEngineCore 6.10 REQUIRED)
find_package(Qt6WebEngineWidgets 6.10 REQUIRED)

set(PANBROWSER_SOURCES
    qt/main.cpp
    qt/MainWindow.cpp
    qt/MainWindow.h
    qt/TrustConfiguration.cpp
    qt/TrustConfiguration.h
    qt/CertificateTrustValidator.h
    qt/resources.qrc
)

if(APPLE)
    set(PANBROWSER_APP_ICON qt/assets/PanBrowser.icns)
    set_source_files_properties(${PANBROWSER_APP_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND PANBROWSER_SOURCES
        qt/CertificateTrustValidator_mac.mm
        ${PANBROWSER_APP_ICON}
    )
else()
    list(APPEND PANBROWSER_SOURCES qt/CertificateTrustValidator_fallback.cpp)
endif()

qt_add_executable(PanBrowserQt MACOSX_BUNDLE ${PANBROWSER_SOURCES})
set_target_properties(PanBrowserQt PROPERTIES
    OUTPUT_NAME PanBrowser
    MACOSX_BUNDLE_BUNDLE_NAME PanBrowser
    MACOSX_BUNDLE_GUI_IDENTIFIER dev.panbrowser.qt
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_ICON_FILE PanBrowser.icns
)

target_include_directories(PanBrowserQt PRIVATE qt)
target_link_libraries(PanBrowserQt PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Svg
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
    Qt6::Widgets
)

if(APPLE)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
    target_link_libraries(PanBrowserQt PRIVATE ${SECURITY_FRAMEWORK})
endif()

enable_testing()
qt_add_executable(PanBrowserQtTests
    qt/tests/TrustConfigurationTests.cpp
    qt/TrustConfiguration.cpp
    qt/TrustConfiguration.h
)
target_include_directories(PanBrowserQtTests PRIVATE qt)
target_link_libraries(PanBrowserQtTests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
add_test(NAME PanBrowserQtTests COMMAND PanBrowserQtTests)
