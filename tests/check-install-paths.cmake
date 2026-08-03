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
if(KDE_INSTALL_APPDIR STREQUAL "")
    message(FATAL_ERROR
        "KDE_INSTALL_APPDIR is empty; the desktop entry would install to the prefix "
        "root, where KWin does not look for it")
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
#
# These match the generated file(INSTALL ...) rule, NOT the bare filename. A
# bare filename match fails open: cmake_install.cmake also carries the text of
# every install(CODE ...) block, and the prefix-override guard in
# src/CMakeLists.txt names both textract.service and the desktop entry in its
# FATAL_ERROR message. So "does the string textract.service appear anywhere in
# this file" stays true after the install() rule is deleted -- the guard would
# report success for exactly the pre-M7b state it exists to prevent (unit
# generated, installed by nothing, every capture NoAuthorized). The match must
# be pinned to a real install rule for the file to mean anything.
#
# The destination is captured rather than baked into the pattern, for two
# reasons. It keeps the regex free of ${...} and other metacharacters that
# would have to survive CMake's own expansion before reaching the regex engine,
# and it lets a wrong DESTINATION report where the file actually lands. That is
# the second half of the check: a rule that installs the unit into, say,
# share/applications is a rule that ships it where systemd will never read it,
# and the old assertion could not tell the difference.
#
# Every wildcard here is [^"]*, never .* -- CMake regexes have no non-greedy
# quantifiers and . matches newlines, so .* would happily span from one rule's
# DESTINATION to a different rule's FILES and call two broken rules one good
# one. [^"]* cannot cross a quote, which confines each match to a single rule.
if(NOT EXISTS "${INSTALL_SCRIPT}")
    message(FATAL_ERROR "no generated install script at ${INSTALL_SCRIPT}")
endif()
file(READ "${INSTALL_SCRIPT}" install_script)

# CMAKE_INSTALL_PREFIX is not expanded when the install script is generated --
# it is read at install time -- so the destination appears in the file as the
# literal characters ${CMAKE_INSTALL_PREFIX}. Reproduce it literally; there is
# no expanded absolute destination in there to match. ECM permits an absolute
# KDE_INSTALL_* dir, in which case CMake emits it with no prefix at all.
if(IS_ABSOLUTE "${KDE_INSTALL_APPDIR}")
    set(expected_desktop_dest "${KDE_INSTALL_APPDIR}")
else()
    set(expected_desktop_dest "\${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_APPDIR}")
endif()
if(IS_ABSOLUTE "${KDE_INSTALL_SYSTEMDUSERUNITDIR}")
    set(expected_unit_dest "${KDE_INSTALL_SYSTEMDUSERUNITDIR}")
else()
    set(expected_unit_dest "\${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_SYSTEMDUSERUNITDIR}")
endif()

if(NOT install_script MATCHES
        "file\\(INSTALL DESTINATION \"([^\"]*)\" TYPE FILE FILES \"[^\"]*/org\\.kde\\.textract\\.desktop\"\\)")
    message(FATAL_ERROR
        "${INSTALL_SCRIPT} has no file(INSTALL ...) rule for org.kde.textract.desktop; "
        "without an installed desktop entry KWin cannot authorise ScreenShot2 for any path")
endif()
set(desktop_dest "${CMAKE_MATCH_1}")
if(NOT desktop_dest STREQUAL "${expected_desktop_dest}")
    message(FATAL_ERROR
        "${INSTALL_SCRIPT} installs the desktop entry to '${desktop_dest}' but KWin "
        "only reads entries under KDE_INSTALL_APPDIR, '${expected_desktop_dest}'")
endif()

if(NOT install_script MATCHES
        "file\\(INSTALL DESTINATION \"([^\"]*)\" TYPE FILE FILES \"[^\"]*/textract\\.service\"\\)")
    message(FATAL_ERROR
        "${INSTALL_SCRIPT} has no file(INSTALL ...) rule for textract.service; the unit "
        "would be generated and then shipped nowhere")
endif()
set(unit_dest "${CMAKE_MATCH_1}")
if(NOT unit_dest STREQUAL "${expected_unit_dest}")
    message(FATAL_ERROR
        "${INSTALL_SCRIPT} installs the systemd unit to '${unit_dest}' but systemd only "
        "reads units under KDE_INSTALL_SYSTEMDUSERUNITDIR, '${expected_unit_dest}'")
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
