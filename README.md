# Ejecutar la GUI (`gui.cpp`) — Windows y Linux (solo terminal)

Este documento explica cómo compilar y ejecutar la **GUI** del proyecto (`gui/gui.cpp`) desde la **terminal**, tanto en **Windows** como en **Linux**. 

---

## 1) Requisitos

### Windows (MSYS2 MinGW64)
1. Instala **MSYS2** y abre **MSYS2 MinGW x64** (no “MSYS”).
2. Instala (si te falta) CMake y Ninja (opcional):
   ```bash
   pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
   ```

> Consejo: Si no instalas Ninja, puedes omitir `-G "Ninja"` y CMake usará Makefiles.

### Linux (para compilar y ejecutar la GUI )
La GUI está escrita con **Win32**; en Linux se compila como ejecutable **Windows** usando **MinGW‑w64** y se ejecuta con **Wine**.

```bash
sudo apt update
sudo apt install cmake mingw-w64 wine
```

---

## 2) Preparación del proyecto

Abre una terminal y sitúate en la **raíz del repo** (donde está `CMakeLists.txt`):

```
MultiProcessor_MESI_DotProduct/
├─ CMakeLists.txt
└─ gui/
   └─ gui.cpp
```

---

## 3) Compilar y ejecutar la GUI

### Windows (MSYS2 MinGW x64)

```bash
# 1) Generar build
cmake -G "Ninja" -S . -B build
# Si no usas Ninja:
# cmake -S . -B build

# 2) Compilar
cmake --build build -j

# 3) Ejecutar la GUI
./gui/output/gui.exe
# (o desde el Explorador: gui/output/gui.exe)
```

> Si al ejecutar desde MSYS muestra error, abre **PowerShell** dentro de la carpeta `gui/output` y lanza `.\gui.exe`.

---

### Linux (compilar con MinGW‑w64 + ejecutar con Wine)

```bash
# 1) Generar build (usa el CMakeLists unificado, no hay que tocar nada)
cmake -S . -B build

# 2) Compilar
cmake --build build -j

# 3) Ejecutar la GUI (Win32) con Wine
cd gui/output
wine gui.exe
```

> El archivo `gui.exe` se genera por un sub‑build interno dirigido a Windows y se deja en `gui/output/` automáticamente.

