# MultiProcessor_MESI_DotProduct - Instrucciones generales

Este repositorio contiene varias implementaciones relacionadas con una simulación multiprocesador y el protocolo MESI. Dependiendo de cuál carpeta abras en tu editor, sigue las instrucciones específicas más abajo para compilar y ejecutar.

Carpetas principales y propósito
- `INTERCONNECT/` : Proyecto CMake para un simulador de interconexión (ejecutable: `multiprocessor_sim`).
- `PE/` : Proyecto CMake para el Processing Element (parecido a `INTERCONNECT`, genera `multiprocessor_sim`).
- `MESI/` : Implementación independiente con un binario sencillo compilado con `g++` o `cl` y salida en `output/`.

Requisitos
- CMake >= 3.10 (para `INTERCONNECT` y `PE`).
- Un compilador C++ compatible con C++17 (g++, clang, MSVC).
- PowerShell (Windows) o una terminal bash (Linux/macOS).

Instrucciones rápidas por carpeta (Windows - PowerShell)

1) INTERCONNECT (CMake)

- Abrir PowerShell en `INTERCONNECT` o abrir la carpeta `INTERCONNECT` en el editor y usar la terminal integrada.
- Crear y entrar en el directorio de build, generar y compilar:

```powershell
mkdir build; cd build
cmake ..
cmake --build .
```

- El ejecutable generado será `multiprocessor_sim.exe` en Windows (o `multiprocessor_sim` en Linux).
- Ejecutarlo desde el directorio `build`:

```powershell
.\multiprocessor_sim.exe
```

2) PE (CMake)

- Flujo idéntico al de `INTERCONNECT` (usa CMake). Desde la carpeta `PE`:

```powershell
mkdir build; cd build
cmake ..
cmake --build .
.\multiprocessor_sim.exe
```

3) MESI (compilación directa)

- La carpeta `MESI` incluye un ejemplo que puede compilarse directamente con `g++` o `cl`. También existe un `build.ps1` opcional.
- Compilar con g++ desde la raíz `MESI`:

```powershell
g++ -std=c++17 -O2 -o .\output\MESI.exe .\src\main.cpp
.\output\MESI.exe
```

