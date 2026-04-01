#include "Buzon.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

const int N = 30;
const int streets = 5;
const int cars = 100;

void carInStreet(Buzon* buzones[], Buzon* mutex, int id) {
    srand(time(NULL) ^ getpid());

    int street = rand() % streets;

    myMessage msg;
    msg.idCarro = id;
    msg.type = street + 1;

    printf("Carro %d llega a calle %d\n", id, street + 1);
    buzones[street]->Enviar(msg);

    myMessage lock;
    mutex->Recibir(lock, 1);

    int total = 0;
    int max = 0;
    int calleMax = 0;

    for (int i = 0; i < streets; i++) {
        int cant = buzones[i]->CantidadMensajes();
        total += cant;

        if (cant > max) {
            max = cant;
            calleMax = i;
        }
    }

    if (total > N) {
        std::cout << "Calle con mas carros: " << calleMax + 1 << " cant " << max << std::endl;
        std::cout << "Entran a la rotonda carros de la calle: " << calleMax + 1 << std::endl;
        int cantidad = buzones[calleMax]->CantidadMensajes();

        for (int i = 0; i < cantidad; i++) {
            myMessage msgRev;
            buzones[calleMax]->Recibir(msgRev, calleMax + 1);
            std::cout << "- Carro " << msgRev.idCarro << " entro a la rotonda " << " [Calle " << calleMax + 1 << "]" << std::endl;
        }
    }
    mutex->Enviar(lock);
}

int main () {

    Buzon* buzones[streets];
    for (int i = 0; i < streets; i++) {
        buzones[i] = new Buzon(i);
    }
    Buzon mutex(999);
    myMessage init;
    init.type = 1;
    mutex.Enviar(init);

    printf("Simulando %d carros en %d calles\n", cars, streets);
    for (int i = 0; i < cars; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            carInStreet(buzones, &mutex, i);
            exit(0);
        }
    }
    for (int i = 0; i < cars; i++) {
        wait(NULL);
    }
    for (int i = 0; i < streets; i++) {
        delete buzones[i];
    }

    return 0;
}