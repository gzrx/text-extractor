# SPDX-FileCopyrightText: 2026 gzrx
# SPDX-License-Identifier: GPL-3.0-or-later

# KWin authorises the restricted ScreenShot2 interface by resolving the calling
# PID to its executable and matching that absolute path against Exec= in an
# installed desktop entry. So the desktop entry, the systemd unit and the
# install location must name the same path, and until M7b they did not: the
# unit said %h/.local/bin/textract while both others said ${KDE_INSTALL_FULL_BINDIR}.
# The result installed cleanly and failed every capture with NoAuthorized.
#
# This runs as a ctest so the agreement is checked on every build rather than
# on every reading of the CMakeLists.

# The generated-file checks below compare template contents against EXPECTED,
# which catches the two files drifting apart. They say nothing about whether
# either file is actually installed, and nothing about where. That was the
# other half of the pre-M7b bug: the unit named the wrong path AND no
# CMakeLists referenced it at all. The install-rule checks come first, so a
# unit that ships nowhere fails here rather than passing on its contents.

if(KDE_INSTALL_BINDIR STREQUAL "")
    message(FATAL_ERROR
        "KDE_INSTALL_BINDIR is empty; the binary would install to the prefix root")
endif()
if(KDE_INSTALL_SYSTEMDUSERUNITDIR STREQUAL "")
    message(FATAL_ERROR
        "KDE_INSTALL_SYSTEMDUSERUNITDIR is empty; the unit would install to the prefix root")
endif()

# EXPECTED is TEXTRACT_EXEC, the string baked into both generated files. The
# binary itself is installed by install(TARGETS) into KDE_INSTALL_BINDIR. Those
# are two independent expressions of one path, so they have to be reconciled --
# pointing TEXTRACT_EXEC at some other install dir would otherwise go unnoticed.
# ECM allows an absolute KDE_INSTALL_BINDIR, in which case the prefix is not
# prepended; mirror that rather than failing a legitimate configuration.
if(IS_ABSOLUTE "${KDE_INSTALL_BINDIR}")
    set(expected_from_dirs "${KDE_INSTALL_BINDIR}/textract")
else()
    set(expected_from_dirs "${INSTALL_PREFIX}/${KDE_INSTALL_BINDIR}/textract")
endif()
if(NOT EXPECTED STREQUAL "${expected_from_dirs}")
    message(FATAL_ERROR
        "TEXTRACT_EXEC is '${EXPECTED}' but install(TARGETS) puts the binary at "
        "'${expected_from_dirs}'")
endif()

# Guards the install() rules themselves: deleting either one leaves every
# content check green while the file ships nowhere.
if(NOT EXISTS "${INSTALL_SCRIPT}")
    message(FATAL_ERROR "no generated install script at ${INSTALL_SCRIPT}")
endif()
file(READ "${INSTALL_SCRIPT}" install_script)
if(NOT install_script MATCHES "org\\.kde\\.textract\\.desktop")
    message(FATAL_ERROR
        "${INSTALL_SCRIPT} installs no desktop entry; without one KWin cannot "
        "authorise ScreenShot2 for any path")
endif()
if(NOT install_script MATCHES "textract\\.service")
    message(FATAL_ERROR
        "${INSTALL_SCRIPT} installs no systemd unit; it would be generated and "
        "then shipped nowhere")
endif()

if(NOT EXISTS "${DESKTOP}")
    message(FATAL_ERROR "desktop entry was not generated at ${DESKTOP}")
endif()
if(NOT EXISTS "${UNIT}")
    message(FATAL_ERROR "systemd unit was not generated at ${UNIT}")
endif()

file(STRINGS "${DESKTOP}" desktop_line REGEX "^Exec=")
file(STRINGS "${UNIT}" unit_line REGEX "^ExecStart=")

if(NOT desktop_line)
    message(FATAL_ERROR "no Exec= line in ${DESKTOP}")
endif()
if(NOT unit_line)
    message(FATAL_ERROR "no ExecStart= line in ${UNIT}")
endif()

string(REGEX REPLACE "^Exec=" "" desktop_path "${desktop_line}")
string(REGEX REPLACE "^ExecStart=" "" unit_path "${unit_line}")
string(REGEX REPLACE " --daemon$" "" desktop_path "${desktop_path}")
string(REGEX REPLACE " --daemon$" "" unit_path "${unit_path}")

if(NOT desktop_path STREQUAL "${EXPECTED}")
    message(FATAL_ERROR
        "desktop Exec= is '${desktop_path}' but the binary installs to '${EXPECTED}'")
endif()
if(NOT unit_path STREQUAL "${EXPECTED}")
    message(FATAL_ERROR
        "unit ExecStart= is '${unit_path}' but the binary installs to '${EXPECTED}'")
endif()

message(STATUS "install paths agree: ${EXPECTED}")
