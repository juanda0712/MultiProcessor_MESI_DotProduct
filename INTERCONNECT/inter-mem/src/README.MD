# Simulador de Arquitectura

Este proyecto es un simulador de arquitectura de computadores escrito en C++. COn este programa se simula el interconnect y la memoria.

## Descripción

Este simulador modela componentes de hardware como memoria e interconexiones, lo que permite simular el comportamiento de un sistema computacional.

## Requisitos

- Compilador C++ compatible con C++17 (g++ recomendado)
- Sistema operativo Linux/Unix
- Make

## Compilación

Para compilar el proyecto, ejecute:

```bash
make
```

Esto generará el ejecutable `sim`.

## Uso

Una vez compilado, ejecute el simulador con:

```bash
./sim
```

## Limpieza

Para limpiar los archivos objeto y el ejecutable:

```bash
make clean
```

## Estructura del proyecto

- `main.cpp` - Punto de entrada principal
- `memory.cpp` - Implementación del subsistema de memoria
- `interconnect.cpp` - Implementación del sistema de interconexión
- `Makefile` - Sistema de construcción

## Características de compilación

- Estándar C++17
- Optimización O2
- Todas las advertencias habilitadas
- Soporte para hilos (pthread)