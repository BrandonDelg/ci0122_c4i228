
/**
  *  C++ program to send messages via operating system message passing queues
  *
  *  Author: Programacion Concurrente (Francisco Arroyo)
  *
  *  Version: 2025/Ago/26
  *
 **/


#define LABEL_SIZE 64
#include <stdio.h>
#include <string.h>
#include <cstdio>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "Buzon.hpp"

const char * html_labels[] = {
   "a",
   "b",
   "c",
   "d",
   "e",
   "li",
   ""
};

int main() {
   
   Buzon m;
   int i = 0;

   while ( strlen(html_labels[ i ] ) ) {
      int st = m.Enviar( html_labels[ i ], LABEL_SIZE, 2026, i);  // Send a message with 2025 type, (label,n)
      printf("Label: %s, status %d \n", html_labels[ i ], st );
      i++;
   }
   return 0;
}