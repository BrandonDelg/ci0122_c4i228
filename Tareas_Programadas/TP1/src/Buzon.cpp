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

Buzon::Buzon(int i) { 
    key_t key = 0xC41228 + i;
    this->id = msgget(key, IPC_CREAT | 0600);
    if (this->id < 0) { 
        throw std::runtime_error("ERROR");
    }
}   

Buzon::~Buzon() { 
    msgctl(this->id, IPC_RMID, NULL); 
}

int Buzon::Enviar(const myMessage& mensaje) { 
    int result = msgsnd(this->id, &mensaje, sizeof(myMessage) - sizeof(long), 0);

    if (result < 0) {
        throw std::runtime_error("Error enviando mensaje");
    }
    return result;
} 


int Buzon::Recibir(myMessage& mensaje, long tipo ) {
    int r = msgrcv(this->id, &mensaje, sizeof(myMessage) - sizeof(long), tipo, 0);
    if (r < 0) {
        throw std::runtime_error("Error recibiendo mensaje");
    }
    return r;
}

int Buzon::CantidadMensajes() {
    struct msqid_ds info;

    if (msgctl(this->id, IPC_STAT, &info) == -1) {
        perror("msgctl");
        return -1;
    }

    return info.msg_qnum;
}
