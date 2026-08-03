set(TEXTRACT_LIB_SOURCES
    capture/rawimage.cpp
    capture/kwincapture.cpp
    overlay/geometry.cpp
    overlay/selectionoverlay.cpp
    preprocess/preprocess.cpp
    ocr/tesseractengine.cpp
    ocr/paddlewords.cpp
    ocr/paddledet.cpp
    ocr/paddlerec.cpp
    ocr/onnxpaddleengine.cpp
    models/modelspec.cpp
    models/fetch.cpp
    analyze/analyze.cpp
    assemble/assemble.cpp
    assemble/columns.cpp
    order/order.cpp
    correct/dictionary.cpp
    config/settings.cpp
    clipboard/clipboard.cpp
    app/extraction.cpp
    app/tier2cache.cpp
    app/extractorcontroller.cpp
)

add_library(textract_lib STATIC ${TEXTRACT_LIB_SOURCES})

# ECM's KDECompilerSettings builds the project with -fno-exceptions, but ONNX
# Runtime's C++ API is exception-based and will not even compile without them
# (onnxruntime_cxx_api.h: "exception handling disabled, use -fexceptions").
#
# Enabled for this ONE translation unit rather than the whole library. That is
# sound only because OnnxPaddleEngine catches every Ort::Exception at its own
# boundary and never lets one propagate into the -fno-exceptions code that
# calls it -- which is also what keeps a corrupt model from taking the daemon
# down. If that containment is ever broken, this scoping breaks with it.
set_source_files_properties(ocr/onnxpaddleengine.cpp
    PROPERTIES COMPILE_OPTIONS "-fexceptions")
set_target_properties(textract_lib PROPERTIES AUTOMOC ON)
target_include_directories(textract_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(textract_lib PUBLIC
    Qt6::Core Qt6::Gui Qt6::DBus Qt6::Network LayerShellQt::Interface
    PkgConfig::Tesseract PkgConfig::Leptonica PkgConfig::Hunspell
    PkgConfig::ONNXRuntime
    KF6::GuiAddons KF6::Notifications KF6::ConfigCore)

# configdialog.cpp is built into the executable rather than textract_lib on
# purpose: every test binary links textract_lib, and putting a Qt Widgets
# dialog there would pull Qt6::Widgets into all of them for code none of them
# use. It defines no QObject subclass, so it needs no MOC.
add_executable(textract main.cpp config/configdialog.cpp)
target_link_libraries(textract PRIVATE
    textract_lib Qt6::Gui Qt6::Widgets KF6::GlobalAccel)

install(TARGETS textract ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# KWin authorises a caller for the restricted ScreenShot2 interface by mapping
# the calling PID to its executable and matching that absolute path against
# Exec= in an installed desktop file. Two consequences, both verified against a
# live KWin 6.7.3:
#   * Exec= must be an ABSOLUTE path to the exact binary being run.
#   * The interface name is case-sensitive and must be org.kde.KWin.ScreenShot2
#     (org.kde.kwin.screenshot is the legacy v1 name and does NOT authorise v2).
set(TEXTRACT_EXEC "${KDE_INSTALL_FULL_BINDIR}/textract")
configure_file(${CMAKE_SOURCE_DIR}/data/org.kde.textract.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/org.kde.textract.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.textract.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})

# The unit is generated from the same TEXTRACT_EXEC as the desktop entry above.
# Before M7b it was a static file naming %h/.local/bin/textract, installed by
# nothing and copied by hand per the README -- so the daemon started from a
# path matching no desktop entry and every capture failed NoAuthorized.
#
# Note KDE_INSTALL_SYSTEMDUSERUNITDIR is ${KDE_INSTALL_LIBDIR}/systemd/user.
# That is right for /usr, but a $HOME/.local prefix puts it in
# ~/.local/lib/systemd/user, which systemd does NOT search (it looks in
# ~/.local/share/systemd/user). The README says so rather than this file
# second-guessing an ECM install variable.
configure_file(${CMAKE_SOURCE_DIR}/data/textract.service.in
               ${CMAKE_CURRENT_BINARY_DIR}/textract.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/textract.service
        DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})

add_test(NAME install_paths_agree
    COMMAND ${CMAKE_COMMAND}
        -DDESKTOP=${CMAKE_CURRENT_BINARY_DIR}/org.kde.textract.desktop
        -DUNIT=${CMAKE_CURRENT_BINARY_DIR}/textract.service
        -DEXPECTED=${TEXTRACT_EXEC}
        -DINSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DKDE_INSTALL_BINDIR=${KDE_INSTALL_BINDIR}
        -DKDE_INSTALL_SYSTEMDUSERUNITDIR=${KDE_INSTALL_SYSTEMDUSERUNITDIR}
        -DINSTALL_SCRIPT=${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake
        -P ${CMAKE_SOURCE_DIR}/tests/check-install-paths.cmake)

# Development entry pointing at the build-tree binary, so the uninstalled build
# can be authorised too. Install it with:
#   cp build/org.kde.textract.dev.desktop ~/.local/share/applications/
file(GENERATE
     OUTPUT ${CMAKE_BINARY_DIR}/org.kde.textract.dev.desktop
     CONTENT
"[Desktop Entry]
Type=Application
Name=Text Extractor (dev build)
Comment=Extract text from a screen region
Exec=$<TARGET_FILE:textract> --daemon
Icon=edit-copy
Terminal=false
NoDisplay=true
X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2
")
