# MESI (C++)

Protocolo MESI implementado en C++.

Este repositorio contiene un programa de ejemplo que simula el comportamiento básico del protocolo de coherencia MESI entre varias caches y una memoria principal.

Contenido
- `src/` - código fuente dividido en varios archivos `.cpp` (ver `src/main.cpp`).
- `build.ps1` - script opcional para compilar en Windows si hay `g++` o `cl.exe` en el PATH.
- `output/` - carpeta donde se coloca el ejecutable tras compilar.

Para ejecutar:

1) Abrir una terminal y situarse en la raíz del repositorio.

2) Compilar con g++. El proyecto actualmente se compila a partir de `src/main.cpp`:

```bash
g++ -std=c++17 -O2 -o ./output/MESI.exe ./src/main.cpp
```

3) Ejecutar el binario (Windows):

```powershell
./output/MESI.exe
```



