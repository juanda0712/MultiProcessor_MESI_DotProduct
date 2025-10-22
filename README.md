# MultiProcessor_MESI_DotProduct

1. Abrir terminal o PowerShell

- **Linux:** Abrir una terminal.  
- **Windows:** Abrir PowerShell o CMD.

2. Crear y entrar al directorio de compilación

mkdir build
cd build

3. Copiar la carpeta "programs" a Build

cp -r ../programs .


4. Generar los archivos de compilación con CMake

cmake ..

5. Compilar el ejecutable

cmake --build .


Linux: Generará multiprocessor_sim

Windows: Generará multiprocessor_sim.exe

6. Ejecutar el simulador

# Linux
./multiprocessor_sim

# Windows
multiprocessor_sim.exe

