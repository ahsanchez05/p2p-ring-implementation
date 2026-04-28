// FUNCIONALIDAD DE LA PARTE SERVIDORA
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/stat.h>
//#include <sys/sendfile.h>
#include <pthread.h>
#include "ring.h"
#include "common.h"


void *request_handler(void *arg) {
    int soc = (long)arg;
    close(soc);
    return NULL;
}

// función para el thread que implementa la funcionalidad de servidor
// debe recibir como argumento el socket de servicio
void *server_thread(void *arg){
    int s = (long)arg;            // socket de servicio
    int s_connec;                   // socket para la conexión con el cliente
    struct sockaddr_in clnt_addr;   // dirección del cliente
    unsigned int addr_size;         // tamaño de la dirección del cliente

    while (1) {
        addr_size = sizeof(clnt_addr);
        s_connec = accept(s, (struct sockaddr *)&clnt_addr, &addr_size);
        if (s_connec < 0) {
            perror("error en accept"); close(s); return 0;
        }

        // crea un thread para atender la petición del cliente
        create_thread(request_handler, (void *)(long)s_connec);
    }
    close(s); // cerramos el socket de servicio, no lo necesitamos
    return NULL;
}