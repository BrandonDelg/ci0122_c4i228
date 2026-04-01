/**
  *   C++ class to encapsulate Unix message passing intrinsic structures and system calls
  *
  *   UCR-ECCI
  *
  *   CI0122 Sistemas Operativos 2026-i
  *
  *   Class implementation
  *
 **/
#include "Buzon.hpp"

Buzon::Buzon() { 
    key_t key = 0xC41228;
    this->id = msgget(key, IPC_CREAT | 0600);
    if (this->id < 0) { 
        throw std::runtime_error("ERROR");
    }
}   

// Buzon::~Buzon() { 
//     msgctl(this->id, IPC_RMID, NULL); 
// }

int Buzon::Enviar(const char * mensaje, long tipo) { 
    struct myMessage x; 
    x.type = tipo; 
    strncpy(x.version, mensaje, 64); 
    int send = msgsnd(this->id, &x, sizeof(x), IPC_NOWAIT); 
    return send; 
} 

int Buzon::Enviar( const void *mensaje, int cantidad, long tipo, int times) { 
    struct myMessage k;
    k.type = tipo; 
    k.times = times; 
    memcpy(&(k.version), mensaje, cantidad); 
    return msgsnd(this->id, &k, sizeof(k), IPC_NOWAIT); 
}

int Buzon::Recibir(void *mensaje, int cantidad, long tipo ) {
    int r = msgrcv(this->id, mensaje, cantidad, tipo, IPC_NOWAIT);
    return r;
}





