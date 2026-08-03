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
