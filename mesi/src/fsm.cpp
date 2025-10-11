// define el conjunto de estados del protocolo MESI y proporciona
// funciones utilitarias para convertir un estado a su representación textual.
#ifndef MESI_FSM_CPP
#define MESI_FSM_CPP

#include <cstdint>

// Enumeración de estados MESI y estados intermedios usados en la máquina de
// estados de las cachés. 
enum class MESI : uint8_t { I, S, E, M, IS, IM, SM, EM };

// Convierte un valor de la enumeración MESI en una cadena constante que
// representa el nombre abreviado del estado. 
static inline const char* mesi_str(MESI s){
    switch(s){
        case MESI::I: return "I"; case MESI::S: return "S"; case MESI::E: return "E"; case MESI::M: return "M";
        case MESI::IS: return "IS"; case MESI::IM: return "IM"; case MESI::SM: return "SM"; case MESI::EM: return "EM";
    }
    return "?";
}

#endif 
