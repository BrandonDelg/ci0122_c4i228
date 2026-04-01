/**
  *  C++ class to encapsulate Unix message passing intrinsic structures and system calls
  *
 **/

#include <unistd.h>	// pid_t definition
#include <sys/types.h>
#include <iostream>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <cstring>
// #define KEY 0xC4I228	// Valor de la llave del recurso

struct myMessage {
   long type;
   int idCarro;
};

class Buzon {
   public:
      Buzon(int i);
      ~Buzon();
      int Enviar(const myMessage& message);
      int Recibir(myMessage&  message, long tipo = 1);	// space in buffer
      int CantidadMensajes();

   private:
      int id;		// Identificador del buzon
};