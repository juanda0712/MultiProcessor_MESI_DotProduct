# GUI (Win32, C++)

Ventana 450x450 dividida en dos cuadros:
- Superior: consola/log (muestra la salida del programa al compilar/ejecutar).
- Inferior: editor de código (editable, con scroll). Incluye botón Examinar para cargar archivo.

Botones:
- Examinar…: abre archivo y lo carga al editor.
- Guardar y compilar: guarda el editor en disco (si no hay ruta, pide Guardar como) y compila `../src/*.cpp` con `-I ../include` a `../build/multiprocessor_sim.exe`.
- Ejecutar: ejecuta el binario compilado y captura su salida al panel de log. Se habilita solo si la compilación fue exitosa.

## Compilación (MSYS2/MinGW)

```bash
# Desde la carpeta raíz del repo
g++ -std=c++17 -municode -O2 -Wall -Wextra -Wl,-subsystem,windows \
  gui/main.cpp -o gui/gui.exe -lole32 -lcomdlg32 -luuid -lshlwapi

# Ejecutar la GUI
./gui/gui.exe
```

Notas:
- La GUI detecta la raíz del repo a partir de su propia ubicación (`gui/..`).
- La compilación de C++ del proyecto se hace invocando `g++` y expandiendo `../src/*.cpp` desde la propia GUI (enumerando archivos), añadiendo `-pthread` y la ruta de includes.
- Se crea `../build/multiprocessor_sim.exe` si no existe la carpeta `build` la crea.
