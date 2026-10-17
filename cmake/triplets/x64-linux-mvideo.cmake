set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# vcpkg's ncurses 6.5 is wide-character only. libmysql calls FindCurses
# without CURSES_NEED_WIDE, so make the hint explicit for this triplet.
list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCURSES_NEED_WIDE=TRUE")