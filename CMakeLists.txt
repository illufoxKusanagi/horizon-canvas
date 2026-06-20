cmake_minimum_required(VERSION 3.16)

project(HorizonCanvas VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES 0)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent)

set(PROJECT_SOURCES
        main.cpp
        resources.qrc
        mainwindow/mainwindow.cpp
        mainwindow/mainwindow.h
        pages/generate_page.cpp
        pages/generate_page.h
        pages/inject_page.cpp
        pages/inject_page.h
        pages/log_page.cpp
        pages/log_page.h
        pages/resume_page.cpp
        pages/resume_page.h
        widgets/image_drop_widget.cpp
        widgets/image_drop_widget.h
        widgets/json_drop_widget.cpp
        widgets/json_drop_widget.h
        widgets/quality_selector.cpp
        widgets/quality_selector.h
        widgets/custom_settings_panel.cpp
        widgets/custom_settings_panel.h
        widgets/zoomable_graphics_view.cpp
        widgets/zoomable_graphics_view.h
        controllers/generation/geometry_json.cpp
        controllers/generation/geometry_json.h
        controllers/generation/shape_generator.cpp
        controllers/generation/shape_generator.h
        controllers/generation/image_math.cpp
        controllers/generation/image_math.h
        controllers/generation/search_engine.h
        controllers/generation/cpu_engine.cpp
        controllers/generation/cpu_engine.h
        controllers/generation/opencl_engine.cpp
        controllers/generation/opencl_engine.h
        controllers/generation/image_preprocessor.cpp
        controllers/generation/image_preprocessor.h
        controllers/injection/fh6_injector.cpp
        controllers/injection/fh6_injector.h
        controllers/injection/rtti_locator.cpp
        controllers/injection/rtti_locator.h
        controllers/injection/win_process.cpp
        controllers/injection/win_process.h
)

# Temporarily commented out to allow building without a valid .ico file.
# Uncomment this block once you copy the real app_icon.ico to the root directory!
if(WIN32)
    set(RESOURCES_RC resources.rc)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(HorizonCanvas
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${RESOURCES_RC}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET HorizonCanvas APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(HorizonCanvas SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(HorizonCanvas
            ${PROJECT_SOURCES}
            ${RESOURCES_RC}
        )
    endif()
endif()
target_link_libraries(HorizonCanvas PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Concurrent "$ENV{WINDIR}/System32/OpenCL.dll" psapi)
target_include_directories(HorizonCanvas PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.HorizonCanvas)
endif()
set_target_properties(HorizonCanvas PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS HorizonCanvas
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(HorizonCanvas)
endif()
