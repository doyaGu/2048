include(FindPackageHandleStandardArgs)

set(_raylib_roots "")

if (DEFINED GAME2048_RAYLIB_HINT_ROOT AND NOT GAME2048_RAYLIB_HINT_ROOT STREQUAL "")
    list(APPEND _raylib_roots "${GAME2048_RAYLIB_HINT_ROOT}")
endif()

if (DEFINED GAME2048_RAYLIB_HINT_DIR AND NOT GAME2048_RAYLIB_HINT_DIR STREQUAL "")
    list(APPEND _raylib_roots
        "${GAME2048_RAYLIB_HINT_DIR}"
        "${GAME2048_RAYLIB_HINT_DIR}/.."
        "${GAME2048_RAYLIB_HINT_DIR}/../..")
endif()

foreach(_root_var IN ITEMS raylib_ROOT Raylib_ROOT)
    if (DEFINED ${_root_var} AND NOT "${${_root_var}}" STREQUAL "")
        list(APPEND _raylib_roots "${${_root_var}}")
    endif()
endforeach()

foreach(_env_var IN ITEMS raylib_ROOT Raylib_ROOT RAYLIB_ROOT)
    if (DEFINED ENV{${_env_var}} AND NOT "$ENV{${_env_var}}" STREQUAL "")
        list(APPEND _raylib_roots "$ENV{${_env_var}}")
    endif()
endforeach()

foreach(_dir_var IN ITEMS raylib_DIR Raylib_DIR)
    if (DEFINED ${_dir_var} AND NOT "${${_dir_var}}" STREQUAL "" AND NOT "${${_dir_var}}" MATCHES "-NOTFOUND$")
        list(APPEND _raylib_roots
            "${${_dir_var}}"
            "${${_dir_var}}/.."
            "${${_dir_var}}/../..")
    endif()
endforeach()

list(REMOVE_DUPLICATES _raylib_roots)

set(_raylib_include_hints "")
set(_raylib_library_hints "")
foreach(_root IN LISTS _raylib_roots)
    list(APPEND _raylib_include_hints
        "${_root}"
        "${_root}/include"
        "${_root}/src")
    list(APPEND _raylib_library_hints
        "${_root}"
        "${_root}/lib"
        "${_root}/lib64"
        "${_root}/src"
        "${_root}/build"
        "${_root}/build/src"
        "${_root}/build/raylib"
        "${_root}/build/raylib/raylib")
endforeach()

find_path(raylib_INCLUDE_DIR
    NAMES raylib.h
    HINTS ${_raylib_include_hints}
    PATH_SUFFIXES include src
)

find_library(raylib_LIBRARY
    NAMES raylib libraylib.a raylib.lib
    HINTS ${_raylib_library_hints}
    PATH_SUFFIXES lib lib64 src build build/src build/raylib build/raylib/raylib
)

set(raylib_INCLUDE_DIRS "${raylib_INCLUDE_DIR}")
set(raylib_LIBRARIES "${raylib_LIBRARY}")

set(Raylib_INCLUDE_DIR "${raylib_INCLUDE_DIR}")
set(Raylib_INCLUDE_DIRS "${raylib_INCLUDE_DIRS}")
set(Raylib_LIBRARY "${raylib_LIBRARY}")
set(Raylib_LIBRARIES "${raylib_LIBRARIES}")

find_package_handle_standard_args(raylib
    REQUIRED_VARS raylib_LIBRARY raylib_INCLUDE_DIR
)

mark_as_advanced(raylib_INCLUDE_DIR raylib_LIBRARY)
mark_as_advanced(Raylib_INCLUDE_DIR Raylib_LIBRARY)

if (raylib_FOUND AND NOT TARGET Raylib::Raylib)
    add_library(Raylib::Raylib UNKNOWN IMPORTED)
    set(_raylib_system_libs "")
    if (WIN32)
        list(APPEND _raylib_system_libs
            winmm
            kernel32
            user32
            gdi32
            winspool
            shell32
            ole32
            oleaut32
            uuid
            comdlg32
            advapi32
            opengl32
            glu32)
    endif()
    set_target_properties(Raylib::Raylib PROPERTIES
        IMPORTED_LOCATION "${raylib_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${raylib_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${_raylib_system_libs}")
endif()

if (raylib_FOUND AND NOT TARGET raylib)
    add_library(raylib INTERFACE IMPORTED)
    set_target_properties(raylib PROPERTIES
        INTERFACE_LINK_LIBRARIES "Raylib::Raylib")
endif()
