cmake_minimum_required(VERSION 3.10)
project(mp_mesi_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# Simulador (común)
# =========================
include_directories(include)
file(GLOB SOURCES src/*.cpp)
add_executable(multiprocessor_sim ${SOURCES})

# =====================================================================
# GUI Win32:
# - En Windows: se compila nativa (tu gui.cpp tal cual).
# - En Linux: desde ESTE MISMO CMakeLists, se hace cross-compile con MinGW.
# =====================================================================

# Para Linux: opción para activar/desactivar el cross-compile de la GUI Win32
if(UNIX AND NOT WIN32)
  option(BUILD_CROSS_WIN_GUI "Build Windows GUI (cross-compile with MinGW-w64) on Linux" ON)
endif()

if(WIN32)
  # =========================
  # WINDOWS NATIVO
  # =========================
  message(STATUS "Target = Windows → Compilando GUI Win32 nativa.")

  # Tu gui.cpp TAL CUAL (Win32)
  add_executable(gui WIN32 gui/gui.cpp)

  # Librerías Win32 que ya usas
  target_link_libraries(gui PRIVATE ole32 comdlg32 uuid shlwapi)

  # Ocultar consola negra (subsystem windows)
  target_link_options(gui PRIVATE -Wl,-subsystem,windows)

  # Dejar el exe en gui/output
  set_target_properties(gui PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/gui/output"
  )

elseif(UNIX)
  # =========================
  # LINUX: cross-compile de GUI Win32 (sin tocar tu gui.cpp)
  # =========================
  if(BUILD_CROSS_WIN_GUI)
    message(STATUS "Linux detectado → Generando GUI Win32 via MinGW-w64 (cross-compile).")

    # Comprobar toolchain MinGW disponible
    find_program(MINGW_CXX x86_64-w64-mingw32-g++)
    if(NOT MINGW_CXX)
      message(FATAL_ERROR "No se encontró x86_64-w64-mingw32-g++. Instala mingw-w64 (e.g., sudo apt install mingw-w64).")
    endif()

    # Elegir generador seguro para el sub-build (evitar fallo si Ninja no está instalado)
    # Usaremos Ninja si existe; de lo contrario, Unix Makefiles.
    find_program(NINJA_EXE ninja)
    if(NINJA_EXE)
      set(_ext_generator "Ninja")
      set(_ext_cmake_args -DCMAKE_MAKE_PROGRAM=${NINJA_EXE})
      message(STATUS "Sub-build GUI: usando generador Ninja")
    else()
      set(_ext_generator "Unix Makefiles")
      set(_ext_cmake_args )
      message(STATUS "Sub-build GUI: Ninja no encontrado → usando Unix Makefiles")
    endif()

    include(ExternalProject)

    # Escribimos un toolchain file dentro del binario-dir (no ensuciamos el repo)
    set(_toolchain "${CMAKE_BINARY_DIR}/mingw64-toolchain.cmake")
    file(WRITE "${_toolchain}" [=[
# ---- Toolchain auto-generado por CMakeLists principal ----
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_AR      ${TOOLCHAIN_PREFIX}-ar)
set(CMAKE_RANLIB  ${TOOLCHAIN_PREFIX}-ranlib)
set(CMAKE_STRIP   ${TOOLCHAIN_PREFIX}-strip)
# ----------------------------------------------------------
]=])

    # Sub-build para Windows con el MISMO source-dir
    set(_win_bin_dir "${CMAKE_BINARY_DIR}/_win_gui_build")

    # Importante: en ese sub-build, CMAKE_SYSTEM_NAME=Windows → se activa rama WIN32 del mismo CMakeLists
    ExternalProject_Add(gui_win_build
      SOURCE_DIR  "${CMAKE_SOURCE_DIR}"
      BINARY_DIR  "${_win_bin_dir}"
      CMAKE_GENERATOR ${_ext_generator}
      CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${_toolchain}
        -DCMAKE_BUILD_TYPE=$<IF:$<CONFIG:>,${CMAKE_BUILD_TYPE},Release>
        ${_ext_cmake_args}
      BUILD_ALWAYS 1
      INSTALL_COMMAND ""
      STEP_TARGETS build
    )

    # El .exe lo dejamos en el mismo sitio que en Windows nativo:
    #   ${CMAKE_SOURCE_DIR}/gui/output/gui.exe
    # Creamos un target "gui" en este super-proyecto que depende del sub-build
    add_custom_target(gui ALL
      DEPENDS gui_win_build
      COMMENT "GUI Win32 cross-compilada con MinGW-w64 quedará en gui/output/gui.exe"
    )

    # Target opcional para ejecutarla con Wine
    find_program(WINE wine)
    if(WINE)
      add_custom_target(run_gui_wine
        COMMAND ${WINE} "${CMAKE_SOURCE_DIR}/gui/output/gui.exe"
        DEPENDS gui
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Ejecutando GUI Win32 con Wine…"
      )
    endif()

  else()
    message(STATUS "BUILD_CROSS_WIN_GUI=OFF → En Linux no se construirá la GUI (solo simulador).")
  endif()
endif()

message(STATUS "CMake configurado.")
