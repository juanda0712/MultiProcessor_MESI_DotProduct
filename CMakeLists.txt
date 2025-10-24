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