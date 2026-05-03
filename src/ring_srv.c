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
#include <stdint.h>
#include "ring.h"
#include "common.h"


// Variables compartidas definidas en ring_cln.c
extern unsigned int g_local_ip;
extern unsigned int g_suc_ip;
extern unsigned short g_suc_port;
extern unsigned short g_local_port;
extern int g_srv_sock;
extern char *g_srd_dir;

void *request_handler(void *arg) {
    int soc = (int)(intptr_t)arg;
    char op;

    if (recv(soc, &op, sizeof(char), MSG_WAITALL) != sizeof(char)) {
        close(soc);
        return NULL;
    }

    switch (op) {
        case 'P': {
            // obtener PID
            int pid = htonl(getpid());
            if (send(soc, &pid, sizeof(pid), 0) != sizeof(pid)) {
                close(soc);
                return NULL;
            }
            break;
        }

        case 'A': {
            // dar de alta nodo nuevo
            unsigned int clnt_ip;
            unsigned short clnt_port;
            unsigned int old_ip;
            unsigned short old_port;
            struct sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);

            // obtener IP del cliente
            if (getpeername(soc, (struct sockaddr *)&addr, &addrlen) < 0) {
                close(soc);
                return NULL;
            }
            clnt_ip = addr.sin_addr.s_addr;

            // recibir puerto del nodo nuevo
            if (recv(soc, &clnt_port, sizeof(clnt_port), MSG_WAITALL) != sizeof(clnt_port)) {
                close(soc);
                return NULL;
            }

            // guardar el sucesor anterior y actualizar el sucesor local
            old_ip = g_suc_ip;
            old_port = g_suc_port;
            g_suc_ip = clnt_ip;
            g_suc_port = clnt_port;

            // responder con el sucesor anterior
            if (send(soc, &old_ip, sizeof(old_ip), 0) != sizeof(old_ip)) {
                close(soc);
                return NULL;
            }
            if (send(soc, &old_port, sizeof(old_port), 0) != sizeof(old_port)) {
                close(soc);
                return NULL;
            }
            break;
        }

        case 'R': {
            // obtener sucesor
            if (send(soc, &g_suc_ip, sizeof(g_suc_ip), 0) != sizeof(g_suc_ip)) {
                close(soc);
                return NULL;
            }
            if (send(soc, &g_suc_port, sizeof(g_suc_port), 0) != sizeof(g_suc_port)) {
                close(soc);
                return NULL;
            }
            break;
        }

        case 'U': {
            // obtener sucesor del sucesor
            unsigned int suc_ip;
            unsigned short suc_port;

            // si el sucesor es él mismo, devolverlo directamente
            if (g_suc_ip == g_local_ip && g_suc_port == g_local_port) {
                suc_ip = g_suc_ip;
                suc_port = g_suc_port;
            } else {
                // pedir el sucesor del sucesor
                if (ring_remote_successor(g_suc_ip, g_suc_port, &suc_ip, &suc_port) < 0) {
                    close(soc);
                    return NULL;
                }
            }

            if (send(soc, &suc_ip, sizeof(suc_ip), 0) != sizeof(suc_ip)) {
                close(soc);
                return NULL;
            }
            if (send(soc, &suc_port, sizeof(suc_port), 0) != sizeof(suc_port)) {
                close(soc);
                return NULL;
            }
            break;
        }

        default:
            break;
    }
    close(soc);
    return NULL;
}

// función para el thread que implementa la funcionalidad de servidor
// debe recibir como argumento el socket de servicio
void *server_thread(void *arg){
    int s = (int)(intptr_t)arg;     // socket de servicio
    int s_connec;                   // socket para la conexión con el cliente
    struct sockaddr_in clnt_addr;   // dirección del cliente
    socklen_t addr_size;            // tamaño de la dirección del cliente

    while (1) {
        addr_size = sizeof(clnt_addr);
        s_connec = accept(s, (struct sockaddr *)&clnt_addr, &addr_size);
        if (s_connec < 0) {
            perror("error en accept"); close(s); return NULL;
        }

        // crea un thread para atender la petición del cliente
        create_thread(request_handler, (void *)(intptr_t)s_connec);
    }
    close(s); // cerramos el socket de servicio, no lo necesitamos
    return NULL;
}