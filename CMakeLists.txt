# No libraries required for now

cmake_minimum_required(VERSION 3.10)
project(mp_mesi_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Incluir encabezados
include_directories(include)

# Buscar todos los archivos fuente automáticamente
file(GLOB SOURCES src/*.cpp)

# Crear ejecutable
add_executable(multiprocessor_sim ${SOURCES})

# =============================
# Sección para compilar la GUI
# =============================
add_executable(gui gui/gui.cpp)
target_link_libraries(gui PRIVATE ole32 comdlg32 uuid shlwapi)

# condicional para multipataforma
if (MINGW)
    target_link_options(gui PRIVATE -Wl,-subsystem,console)
endif()

# Deja el ejecutable en /gui/output
set_target_properties(gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/gui/output"
)
