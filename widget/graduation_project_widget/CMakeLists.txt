cmake_minimum_required(VERSION 3.16)

project(graduation_project_widget VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(SHARED_LOGGER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../shared")

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        login_page.cpp
        login_page.h
        dashboard_page.cpp
        dashboard_page.h
        patient_page.cpp
        patient_page.h
        alarm_center_page.cpp
        alarm_center_page.h
        alert_detail_dialog.cpp
        alert_detail_dialog.h
        about_page.cpp
        about_page.h
        sidebar_widget.cpp
        sidebar_widget.h
        pagination_widget.cpp
        pagination_widget.h
        csv_exporter.cpp
        csv_exporter.h
        business_client.cpp
        business_client.h
        inferpage.cpp
        inferpage.h
        inferpage.ui
        ecg_wave_widget.cpp
        ecg_wave_widget.h
        tcpmgr.cpp
        tcpmgr.h
        global.h
        singleton.h
        resources.qrc
        ${SHARED_LOGGER_DIR}/logger.cpp
        ${SHARED_LOGGER_DIR}/logger.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(graduation_project_widget
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(graduation_project_widget SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(graduation_project_widget
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(graduation_project_widget PRIVATE "${SHARED_LOGGER_DIR}")

target_link_libraries(graduation_project_widget
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.graduation_project_widget)
endif()
set_target_properties(graduation_project_widget PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS graduation_project_widget
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(graduation_project_widget)
endif()
