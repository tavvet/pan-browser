cmake_minimum_required(VERSION 3.24)

project(PanBrowser VERSION 0.2.0 LANGUAGES CXX)

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

find_package(Qt6 6.11 REQUIRED COMPONENTS
    Core
    Network
    Sql
    Svg
    Test
    Widgets
)
find_package(Qt6WebEngineCore 6.11 REQUIRED)
find_package(Qt6WebEngineWidgets 6.11 REQUIRED)

set(PANBROWSER_APP_SOURCES
    src/AddressLineEdit.cpp
    src/AddressLineEdit.h
    src/BrowserDataCleanup.cpp
    src/BrowserDataCleanup.h
    src/BrowserProfile.cpp
    src/BrowserProfile.h
    src/BrowserPreferences.cpp
    src/BrowserPreferences.h
    src/BrowserPage.cpp
    src/BrowserPage.h
    src/DownloadHistoryStore.cpp
    src/DownloadHistoryStore.h
    src/DownloadManager.cpp
    src/DownloadManager.h
    src/DownloadsPanel.cpp
    src/DownloadsPanel.h
    src/ExternalNavigationPolicy.cpp
    src/ExternalNavigationPolicy.h
    src/HistoryStore.cpp
    src/HistoryStore.h
    src/HistoryCompletionPopup.cpp
    src/HistoryCompletionPopup.h
    src/HistorySettingsPage.cpp
    src/HistorySettingsPage.h
    src/PermissionController.cpp
    src/PermissionController.h
    src/PermissionPolicy.cpp
    src/PermissionPolicy.h
    src/PermissionPrompt.cpp
    src/PermissionPrompt.h
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SessionStore.cpp
    src/SessionStore.h
    src/SearchSettings.cpp
    src/SearchSettings.h
    src/SearchSettingsPage.cpp
    src/SearchSettingsPage.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/TrustConfiguration.cpp
    src/TrustConfiguration.h
    src/TrustRulesDialog.cpp
    src/TrustRulesDialog.h
    src/TrustSettings.cpp
    src/TrustSettings.h
    src/WindowPlacement.cpp
    src/WindowPlacement.h
    src/CertificateTrustValidator.h
    src/resources.qrc
)

if(APPLE)
    set(PANBROWSER_APP_ICON src/assets/PanBrowser.icns)
    set_source_files_properties(${PANBROWSER_APP_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND PANBROWSER_APP_SOURCES
        src/CertificateTrustValidatorMac.mm
        ${PANBROWSER_APP_ICON}
    )
else()
    list(APPEND PANBROWSER_APP_SOURCES src/CertificateTrustValidatorFallback.cpp)
endif()

qt_add_executable(PanBrowser MACOSX_BUNDLE ${PANBROWSER_APP_SOURCES})
set_target_properties(PanBrowser PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME PanBrowser
    MACOSX_BUNDLE_GUI_IDENTIFIER dev.panbrowser.app
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_ICON_FILE PanBrowser.icns
)

target_include_directories(PanBrowser PRIVATE src)
target_link_libraries(PanBrowser PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Svg
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
    Qt6::Widgets
)

if(APPLE)
    set_target_properties(PanBrowser PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in
    )
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
    target_link_libraries(PanBrowser PRIVATE ${SECURITY_FRAMEWORK})
endif()

enable_testing()
qt_add_executable(PanBrowserTests
    tests/TrustConfigurationTests.cpp
    src/AddressLineEdit.cpp
    src/AddressLineEdit.h
    src/BrowserDataCleanup.cpp
    src/BrowserDataCleanup.h
    src/BrowserPreferences.cpp
    src/BrowserPreferences.h
    src/DownloadHistoryStore.cpp
    src/DownloadHistoryStore.h
    src/ExternalNavigationPolicy.cpp
    src/ExternalNavigationPolicy.h
    src/HistoryStore.cpp
    src/HistoryStore.h
    src/PermissionPolicy.cpp
    src/PermissionPolicy.h
    src/SessionStore.cpp
    src/SessionStore.h
    src/SearchSettings.cpp
    src/SearchSettings.h
    src/TrustConfiguration.cpp
    src/TrustConfiguration.h
    src/TrustSettings.cpp
    src/TrustSettings.h
    src/WindowPlacement.cpp
    src/WindowPlacement.h
)
target_include_directories(PanBrowserTests PRIVATE src)
target_link_libraries(PanBrowserTests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME PanBrowserTests COMMAND PanBrowserTests)
