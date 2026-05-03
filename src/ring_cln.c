// FUNCIONALIDAD DE LA PARTE CLIENTE
#include <sys/mman.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "ring.h"
#include "common.h"


// Variables globales
unsigned int g_local_ip = 0;
unsigned int g_suc_ip = 0;
unsigned short g_suc_port = 0;
unsigned short g_local_port = 0;
int g_srv_sock = -1;
char *g_srd_dir = NULL;


// Interfaz de funciones auxiliares
static int is_initialized(void);
static int initialize(void);


// inicia el nodo añadiéndolo a la red P2P si ya está creada;
// los puertos e IPs deben estar en formato de red;
// debe devolver en el último parámetro el puerto reservado en formato red;
// retorna 0 si OK y -1 si error
int ring_init(const char *shrd_dir, unsigned int local_ip, unsigned int remote_ip, unsigned short remote_port, unsigned short *alloc_port) {
    int soc;
    char op;
    // unsigned short suc_port;
    // unsigned int suc_ip;

    if (initialize()) return -1; // ya está inicializada
    if (!shrd_dir || !alloc_port) return -1; // parámetros no válidos

    // guarda la IP local y el directorio compartido
    g_local_ip = local_ip;
    g_srd_dir = strdup(shrd_dir);
    if (!g_srd_dir) return -1; // error de memoria

    // crea el socket de servicio y obtiene el puerto asignado
    g_srv_sock = create_socket_srv(&g_local_port);
    if (g_srv_sock < 0) {
        free(g_srd_dir);
        g_srd_dir = NULL;
        return -1;
    }

    *alloc_port = g_local_port; // devuelve el puerto asignado

    // primer nodo: sucesor es él mismo
    if (remote_ip == 0 && remote_port == 0) {
        g_suc_ip = g_local_ip;
        g_suc_port = g_local_port;
    } else {
        // nodo que se une a la red: debe contactar con el nodo remoto para obtener su sucesor y actualizarlo
        soc = create_socket_cln(remote_ip, remote_port);
        if (soc < 0) {
            close(g_srv_sock); g_srv_sock = -1;
            free(g_srd_dir); g_srd_dir = NULL;
            return -1; // error en conexión
        }

        // manda código de alta
        op = 'A';
        if (send(soc, &op, sizeof(char), 0) != sizeof(char)) {
            close(soc);
            close(g_srv_sock);
            g_srv_sock = -1;
            free(g_srd_dir);
            g_srd_dir = NULL;
            return -1;
        }

        // manda el puerto del nuevo nodo
        if (send(soc, &g_local_port, sizeof(g_local_port), 0) != sizeof(g_local_port)) {
            close(soc);
            close(g_srv_sock);
            g_srv_sock = -1;
            free(g_srd_dir);
            g_srd_dir = NULL;
            return -1;
        }

        // recibe el sucesor anterior del nodo de contacto
        if (recv(soc, &g_suc_ip, sizeof(g_suc_ip), MSG_WAITALL) != sizeof(g_suc_ip)) {
            close(soc);
            close(g_srv_sock);
            g_srv_sock = -1;
            free(g_srd_dir);
            g_srd_dir = NULL;
            return -1;
        }
        if (recv(soc, &g_suc_port, sizeof(g_suc_port), MSG_WAITALL) != sizeof(g_suc_port)) {
            close(soc);
            close(g_srv_sock);
            g_srv_sock = -1;
            free(g_srd_dir);
            g_srd_dir = NULL;
            return -1;
        }

        close(soc);

        // g_suc_ip = suc_ip;
        // g_suc_port = suc_port;
    }

    // crea el thread de servicio
    if (create_thread(server_thread, (void *)(long)g_srv_sock) != 0) {
        close(g_srv_sock);
        g_srv_sock = -1;
        free(g_srd_dir);
        g_srd_dir = NULL;
        return -1;
    }
    return 0;
}


// función local que devuelve la IP y el puerto del nodo;
// retorna 0 si OK y -1 si error
int ring_self(unsigned int *ip, unsigned short *port) {
    if (!is_initialized()) return -1; // no está inicializada
    if (!ip || !port) return -1;
    *ip = g_local_ip;
    *port = g_local_port;
    return 0;
}


// devuelve el PID del nodo remoto especificado o -1 si error
int ring_remote_pid(unsigned int remote_ip, unsigned short remote_port) {
    if (!is_initialized()) return -1; // no está inicializada

    int soc = create_socket_cln(remote_ip, remote_port);
    if (soc < 0) return -1; // error en conexión

    char op = 'P';
    if (send(soc, &op, sizeof(char), 0) != sizeof(op)) {    // envía la operación
        close(soc);
        return -1;
    }

    int pid;
    if (recv(soc, &pid, sizeof(int), MSG_WAITALL) != sizeof(int)) { // recibe el PID
        close(soc);
        return -1;
    }
    close(soc);
    return ntohl(pid); // devuelve el PID en formato de host
}


// función local que devuelve la IP y el puerto del nodo sucesor;
// retorna 0 si OK y -1 si error
int ring_successor(unsigned int *ip, unsigned short *port) {
    if (!is_initialized()) return -1; // no está inicializada
    if (!ip || !port) return -1;
    *ip = g_suc_ip;
    *port = g_suc_port;
    return 0;
}


// devuelve la IP y el puerto del nodo sucesor del especificado;
// retorna 0 si OK y -1 si error
int ring_remote_successor(unsigned int remote_ip, unsigned short remote_port, unsigned int *suc_ip, unsigned short *suc_port) {
    int soc;
    char op;

    if (!is_initialized()) return -1; // no está inicializada
    if (!suc_ip || !suc_port) return -1; // parámetros no válidos

    soc = create_socket_cln(remote_ip, remote_port);
    if (soc < 0) return -1;

    op = 'R';
    if (send(soc, &op, sizeof(char), 0) != sizeof(char)) {
        close(soc);
        return -1;
    }

    if (recv(soc, suc_ip, sizeof(*suc_ip), MSG_WAITALL) != sizeof(*suc_ip)) {
        close(soc);
        return -1;
    }

    if (recv(soc, suc_port, sizeof(*suc_port), MSG_WAITALL) != sizeof(*suc_port)) {
        close(soc);
        return -1;
    }

    close(soc);
    return 0;
}


// devuelve la IP y el puerto del nodo sucesor del sucesor del especificado;
// retorna 0 si OK y -1 si error
int ring_remote_successor_successor(unsigned int remote_ip, unsigned short remote_port, unsigned int *suc_suc_ip, unsigned short *suc_suc_port) {
    int soc;
    char op;

    if (!is_initialized()) return -1; // no está inicializada

    soc = create_socket_cln(remote_ip, remote_port);
    if (soc < 0) return -1;

    op = 'U';
    if (send(soc, &op, sizeof(char), 0) != sizeof(char)) {
        close(soc);
        return -1;
    }

    if (recv(soc, suc_suc_ip, sizeof(*suc_suc_ip), MSG_WAITALL) != sizeof(*suc_suc_ip)) {
        close(soc);
        return -1;
    }

    if (recv(soc, suc_suc_port, sizeof(*suc_suc_port), MSG_WAITALL) != sizeof(*suc_suc_port)) {
        close(soc);
        return -1;
    }

    close(soc);
    return 0;
}


// descarga el fichero del nodo especificado;
// retorna el tamaño del fichero si OK y -1 en caso de error
int ring_download(unsigned int remote_ip, unsigned short remote_port, const char *filename) {
    if (!is_initialized()) return -1; // no está inicializada
    return 0;
}
// busca el fichero en el anillo dando un número máximo de saltos y devolviendo
// la IP y el puerto del nodo que lo contiene;
// retorna 0 si OK y -1 si error
int ring_lookup(const char *filename, int hops, unsigned int *ip, unsigned short *port) {
    if (!is_initialized()) return -1; // no está inicializada
    return 0;
}
// busca y descarga el fichero del nodo encontrado en el anillo que lo contiene;
// retorna el tamaño del fichero si OK y -1 en caso de error;
// ESTA FUNCIÓN YA ESTA COMPLETADA
int ring_get_file(const char *filename, int hops) {
    if (!is_initialized()) return -1; // no está inicializada
    unsigned int ip, ip_local;
    unsigned short port, port_local;
    int res = ring_lookup(filename, hops, &ip, &port);
    ring_self(&ip_local, &port_local);
    // realiza la descarga si encontrado en un nodo que no es el local
    if ((res!=-1) && ((ip!=ip_local) || (port!=port_local)))
        res = ring_download(ip, port, filename);
    return res;
}

// funciones auxiliares
static int initialized;

static int initialize(void) {
    return initialized?1:(initialized=1,0);
}
static int is_initialized(void) {
    return initialized;
}
