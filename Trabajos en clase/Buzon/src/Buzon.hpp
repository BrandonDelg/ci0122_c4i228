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
   int times;
   char version[64];
};

class Buzon {
   public:
      Buzon();
      // ~Buzon();
      int Enviar(const char * message, long tipo);
      int Enviar(const void *mensaje, int cantidad, long tipo, int times = 0);
      int Recibir(void *mensaje, int cantidad, long tipo = 1);	// space in buffer

   private:
      int id;		// Identificador del buzon
};