cmake_minimum_required(VERSION 3.28)
project(textract VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets DBus Test)
find_package(KF6 6.28 REQUIRED COMPONENTS
    GlobalAccel GuiAddons Config Notifications I18n WindowSystem)
find_package(LayerShellQt REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(Tesseract REQUIRED IMPORTED_TARGET tesseract)
pkg_check_modules(Leptonica REQUIRED IMPORTED_TARGET lept)
pkg_check_modules(Hunspell REQUIRED IMPORTED_TARGET hunspell)

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
