#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "Buzon.hpp"

#define LABEL_SIZE 64

int main() {
   struct msgbuf {
      long mtype;     // message type, must be > 0 
      int times;	// Times that label appears
      char label[ LABEL_SIZE ];  // Label to send to mailbox
   };

   struct msgbuf A;
   Buzon* m = new Buzon();
   int st = m->Recibir( &A, sizeof(A), 2026 );
   while (st > 0) { 
      printf("Label: %s, times %d\n", A.label, A.times );
      st = m->Recibir( &A, sizeof(A), 2026 );
   }
}