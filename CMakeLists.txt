cmake_minimum_required(VERSION 3.24)

project(PanBrowser VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)

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
    LinguistTools
    Widgets
)
find_package(Qt6WebEngineCore 6.11 REQUIRED)
find_package(Qt6WebEngineWidgets 6.11 REQUIRED)

set(PANBROWSER_APP_SOURCES
    src/ApplicationLaunch.cpp
    src/ApplicationLaunch.h
    src/AddressLineEdit.cpp
    src/AddressLineEdit.h
    src/AddressSuggestion.cpp
    src/AddressSuggestion.h
    src/BookmarkStore.cpp
    src/BookmarkStore.h
    src/BookmarkDialog.cpp
    src/BookmarkDialog.h
    src/BookmarksDialog.cpp
    src/BookmarksDialog.h
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
    src/DiagnosticsPage.cpp
    src/DiagnosticsPage.h
    src/DnsSettings.cpp
    src/DnsSettings.h
    src/DnsSettingsPage.cpp
    src/DnsSettingsPage.h
    src/ExternalNavigationPolicy.cpp
    src/ExternalNavigationPolicy.h
    src/FindBar.cpp
    src/FindBar.h
    src/HistoryStore.cpp
    src/HistoryStore.h
    src/Localization.cpp
    src/Localization.h
    src/AddressCompletionPopup.cpp
    src/AddressCompletionPopup.h
    src/HistorySettingsPage.cpp
    src/HistorySettingsPage.h
    src/PermissionController.cpp
    src/PermissionController.h
    src/PermissionPolicy.cpp
    src/PermissionPolicy.h
    src/PermissionPrompt.cpp
    src/PermissionPrompt.h
    src/ProxyAuthenticationController.cpp
    src/ProxyAuthenticationController.h
    src/ProxySettings.cpp
    src/ProxySettings.h
    src/ProxySettingsPage.cpp
    src/ProxySettingsPage.h
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
    src/WebAppStore.cpp
    src/WebAppStore.h
    src/WebAppsSettingsPage.cpp
    src/WebAppsSettingsPage.h
    src/WebAppShortcutManager.cpp
    src/WebAppShortcutManager.h
    src/WindowPlacement.cpp
    src/WindowPlacement.h
    src/WindowChrome.cpp
    src/WindowChrome.h
    src/WindowChromePlatform.h
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
        src/MacApplicationReopen.h
        src/MacApplicationReopen.mm
        src/WindowChromeMac.mm
        ${PANBROWSER_APP_ICON}
    )
elseif(WIN32)
    list(APPEND PANBROWSER_APP_SOURCES src/CertificateTrustValidatorWindows.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND PANBROWSER_APP_SOURCES src/CertificateTrustValidatorLinux.cpp)
else()
    list(APPEND PANBROWSER_APP_SOURCES src/CertificateTrustValidatorFallback.cpp)
endif()

if(NOT APPLE)
    list(APPEND PANBROWSER_APP_SOURCES src/WindowChromePlatform.cpp)
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

    add_executable(PanBrowserWebAppLauncher src/WebAppLauncherMac.cpp)
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(PanBrowserWebAppLauncher PRIVATE ${CORE_FOUNDATION_FRAMEWORK})
    set(PANBROWSER_WEB_APP_LAUNCHER_RESOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/PanBrowserWebAppLauncher"
    )
    add_custom_command(
        OUTPUT "${PANBROWSER_WEB_APP_LAUNCHER_RESOURCE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/generated"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:PanBrowserWebAppLauncher>"
            "${PANBROWSER_WEB_APP_LAUNCHER_RESOURCE}"
        DEPENDS PanBrowserWebAppLauncher
        VERBATIM
    )
    set_source_files_properties(
        "${PANBROWSER_WEB_APP_LAUNCHER_RESOURCE}"
        PROPERTIES
            GENERATED TRUE
            MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(PanBrowser PRIVATE "${PANBROWSER_WEB_APP_LAUNCHER_RESOURCE}")
endif()

if(WIN32)
    target_compile_definitions(PanBrowser PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        CERT_CHAIN_PARA_HAS_EXTRA_FIELDS
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(PanBrowser PRIVATE Crypt32)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(OpenSSL 1.1.1 REQUIRED COMPONENTS Crypto)
    target_link_libraries(PanBrowser PRIVATE OpenSSL::Crypto)
endif()

if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(TARGETS PanBrowser
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(FILES README.md THIRD_PARTY_NOTICES.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/PanBrowser
    )
    qt_generate_deploy_app_script(
        TARGET PanBrowser
        OUTPUT_SCRIPT PANBROWSER_DEPLOY_SCRIPT
    )
    install(SCRIPT ${PANBROWSER_DEPLOY_SCRIPT})
endif()

enable_testing()
qt_add_executable(PanBrowserTests
    src/ApplicationLaunch.cpp
    src/ApplicationLaunch.h
    src/AddressCompletionPopup.cpp
    src/AddressCompletionPopup.h
    tests/TrustConfigurationTests.cpp
    src/AddressLineEdit.cpp
    src/AddressLineEdit.h
    src/AddressSuggestion.cpp
    src/AddressSuggestion.h
    src/BookmarkStore.cpp
    src/BookmarkStore.h
    src/BrowserDataCleanup.cpp
    src/BrowserDataCleanup.h
    src/BrowserProfile.cpp
    src/BrowserProfile.h
    src/BrowserPreferences.cpp
    src/BrowserPreferences.h
    src/DownloadHistoryStore.cpp
    src/DownloadHistoryStore.h
    src/DnsSettings.cpp
    src/DnsSettings.h
    src/ExternalNavigationPolicy.cpp
    src/ExternalNavigationPolicy.h
    src/FindBar.cpp
    src/FindBar.h
    src/HistoryStore.cpp
    src/HistoryStore.h
    src/Localization.cpp
    src/Localization.h
    src/PermissionPolicy.cpp
    src/PermissionPolicy.h
    src/ProxySettings.cpp
    src/ProxySettings.h
    src/SessionStore.cpp
    src/SessionStore.h
    src/SearchSettings.cpp
    src/SearchSettings.h
    src/TrustConfiguration.cpp
    src/TrustConfiguration.h
    src/TrustSettings.cpp
    src/TrustSettings.h
    src/WebAppStore.cpp
    src/WebAppStore.h
    src/WebAppShortcutManager.cpp
    src/WebAppShortcutManager.h
    src/WindowPlacement.cpp
    src/WindowPlacement.h
    src/WindowChrome.cpp
    src/WindowChrome.h
    src/WindowChromePlatform.h
)
target_include_directories(PanBrowserTests PRIVATE src)
target_link_libraries(PanBrowserTests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Test
    Qt6::WebEngineCore
    Qt6::Widgets
)

if(APPLE)
    target_sources(PanBrowserTests PRIVATE src/WindowChromeMac.mm)
    add_dependencies(PanBrowserTests PanBrowser)
else()
    target_sources(PanBrowserTests PRIVATE src/WindowChromePlatform.cpp)
endif()

if(WIN32)
    target_sources(PanBrowserTests PRIVATE src/CertificateTrustValidatorWindows.cpp)
    target_compile_definitions(PanBrowserTests PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        CERT_CHAIN_PARA_HAS_EXTRA_FIELDS
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(PanBrowserTests PRIVATE Crypt32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(PanBrowserTests PRIVATE src/CertificateTrustValidatorLinux.cpp)
    target_link_libraries(PanBrowserTests PRIVATE OpenSSL::Crypto)
endif()

qt_add_translations(
    TARGETS PanBrowser PanBrowserTests
    TS_FILES translations/panbrowser_ru.ts
    PLURALS_TS_FILE translations/panbrowser_en.ts
    SOURCES
        ${PANBROWSER_APP_SOURCES}
        src/CertificateTrustValidatorFallback.cpp
        src/CertificateTrustValidatorLinux.cpp
        src/CertificateTrustValidatorMac.mm
        src/CertificateTrustValidatorWindows.cpp
    RESOURCE_PREFIX "/i18n"
)

add_test(NAME PanBrowserTests COMMAND PanBrowserTests)
