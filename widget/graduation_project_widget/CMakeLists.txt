cmake_minimum_required(VERSION 3.16)

project(graduation_project_widget VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Qml Network Multimedia)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Qml Network Multimedia)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(SHARED_LOGGER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../shared")

qt_add_executable(graduation_project_widget
    main.cpp
    business_client.cpp
    business_client.h
    tcpmgr.cpp
    tcpmgr.h
    global.h
    singleton.h
    Backend/AlertManager.cpp
    Backend/AlertManager.h
    Backend/AuthService.cpp
    Backend/AuthService.h
    Backend/NetworkService.cpp
    Backend/NetworkService.h
    Models/AlertModel.cpp
    Models/AlertModel.h
    Models/EcgDataStream.cpp
    Models/EcgDataStream.h
    Models/HistoryModel.cpp
    Models/HistoryModel.h
    Models/PatientModel.cpp
    Models/PatientModel.h
    Models/TimelineModel.cpp
    Models/TimelineModel.h
    ViewModels/AppViewModel.cpp
    ViewModels/AppViewModel.h
    ViewModels/DashboardViewModel.cpp
    ViewModels/DashboardViewModel.h
    ViewModels/MonitorViewModel.cpp
    ViewModels/MonitorViewModel.h
    ${SHARED_LOGGER_DIR}/logger.cpp
    ${SHARED_LOGGER_DIR}/logger.h
)

qt_add_qml_module(graduation_project_widget
    URI graduation_project.widget
    VERSION 1.0
    QML_FILES
        UI/Main.qml
        UI/Components/AppButton.qml
        UI/Components/AppComboBox.qml
        UI/Components/AppTextArea.qml
        UI/Components/AppTextField.qml
        UI/Components/ConfirmDialog.qml
        UI/Components/Sidebar.qml
        UI/Components/TopBar.qml
        UI/Components/Toast.qml
        UI/Components/MetricCard.qml
        UI/Components/SectionCard.qml
        UI/Components/StatusPill.qml
        UI/Components/WaveCanvas.qml
        UI/Components/LineChart.qml
        UI/Components/DonutChart.qml
        UI/Components/RadarChart.qml
        UI/Pages/LoginPage.qml
        UI/Pages/DashboardPage.qml
        UI/Pages/PatientPage.qml
        UI/Pages/MonitorPage.qml
        UI/Pages/AlertCenterPage.qml
        UI/Pages/AboutPage.qml
    RESOURCES
        UI/Theme.js
        data/model_metrics.json
        UI/audio/critical_alert.wav
)

target_include_directories(graduation_project_widget PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SHARED_LOGGER_DIR}"
)

target_link_libraries(graduation_project_widget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::Network
    Qt6::Multimedia
)

set_target_properties(graduation_project_widget PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

include(GNUInstallDirs)
install(TARGETS graduation_project_widget
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
