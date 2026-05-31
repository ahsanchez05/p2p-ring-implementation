// FUNCIONALIDAD DE LA PARTE SERVIDORA
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include "ring.h"
#include "common.h"


// Variables compartidas definidas en ring_cln.c
extern unsigned int g_local_ip;
extern unsigned int g_suc_ip;
extern unsigned short g_suc_port;
extern unsigned short g_local_port;
extern int g_srv_sock;
extern char *g_srd_dir;


// Manejador de solicitudes que procesa operaciones del cliente
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

            // responder con el sucesor anterior (envío en dos pasos usando MSG_MORE)
            if (send(soc, &old_ip, sizeof(old_ip), MSG_MORE) != sizeof(old_ip)) {
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
            if (send(soc, &g_suc_ip, sizeof(g_suc_ip), MSG_MORE) != sizeof(g_suc_ip)) {
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

            if (send(soc, &suc_ip, sizeof(suc_ip), MSG_MORE) != sizeof(suc_ip)) {
                close(soc);
                return NULL;
            }
            if (send(soc, &suc_port, sizeof(suc_port), 0) != sizeof(suc_port)) {
                close(soc);
                return NULL;
            }
            break;
        }

        case 'D': {
            // Descarga solicitada por el cliente
            uint32_t filename_len_net;

            // Recibir tamaño del fichero
            if (recv(soc, &filename_len_net, sizeof(filename_len_net), MSG_WAITALL) != sizeof(filename_len_net)) {
                close(soc);
                return NULL;
            }

            // Pasar longitud de fichero a formato de host
            uint32_t filename_len = ntohl(filename_len_net);
            if (filename_len == 0 || filename_len > PATH_MAX) {
                close(soc);
                return NULL;
            }

            char *file_name = (char *)malloc(filename_len + 1);
            if (!file_name) {
                close(soc);
                return NULL;
            }

            // Recibir nombre del fichero
            if (recv(soc, file_name, filename_len, MSG_WAITALL) != filename_len) {
                free(file_name);
                close(soc);
                return NULL;
            }
            file_name[filename_len] = '\0'; // Asegurar terminación de cadena

            // construir ruta completa del fichero
            char path[PATH_MAX];
            if (snprintf(path, sizeof(path), "%s/%s", g_srd_dir ? g_srd_dir : ".", file_name) >= (int)sizeof(path)) {
                free(file_name);
                close(soc);
                return NULL;
            }

            // abrir fichero
            int fd = open(path, O_RDONLY);
            if (fd < 0) {
                int32_t err = htonl(-1);
                send(soc, &err, sizeof(err), 0);
                free(file_name);
                close(soc);
                return NULL;
            }


            // fstat para obtener metadatos del fichero
            struct stat st;
            if (fstat(fd, &st) < 0) {
                close(fd);
                int32_t err = htonl(-1);
                send(soc, &err, sizeof(err), 0);
                free(file_name);
                close(soc);
                return NULL;
            }

            off_t file_size = st.st_size;
            if (file_size < 0) file_size = 0;

            uint32_t fsize32 = (uint32_t)file_size;
            uint32_t fsize32_net = htonl(fsize32);

            // Envia el tamaño del fichero
            if (send(soc, &fsize32_net, sizeof(fsize32_net), MSG_MORE) != sizeof(fsize32_net)) {
                close(fd);
                free(file_name);
                close(soc);
                return NULL;
            }

            // Envío del fichero con sendfile (zerocopy)
            off_t offset = 0;
            while (offset < (off_t)fsize32) {
                ssize_t sent = sendfile(soc, fd, &offset, (size_t)((off_t)fsize32 - offset)); // solo compila en linux
                if (sent > 0) {
                    continue;
                }
                if (sent == 0) {
                    break; // EOF
                }
                if (errno == EINTR) {
                    continue; // reintentar si se interrumpe
                }
                // error
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // socket no listo para enviar, esperar un poco y reintentar
                    usleep(1000); // esperar 1ms antes de reintentar
                    continue;
                }
                // error
                close(fd);
                free(file_name);
                close(soc);
                return NULL;
            }
            close(fd);
            free(file_name);
            close(soc);
            return NULL;
        }

        case 'L': {
            // buscar fichero en el anillo

            // recibir longitud del fichero
            uint32_t filename_len_net;
            if (recv(soc, &filename_len_net, sizeof(filename_len_net), MSG_WAITALL) != sizeof(filename_len_net)) {
                close(soc);
                return NULL;
            }

            // Pasar filename_len a formato de host
            uint32_t filename_len = ntohl(filename_len_net);
            if (filename_len == 0 || filename_len > PATH_MAX) {
                close(soc);
                return NULL;
            }

            // recibir nombre del fichero
            char *file_name = (char *)malloc(filename_len + 1);
            if (!file_name) {
                close(soc);
                return NULL;
            }

            if (recv(soc, file_name, filename_len, MSG_WAITALL) != filename_len) {
                free(file_name);
                close(soc);
                return NULL;
            }
            file_name[filename_len] = '\0'; // Asegurar terminación de cadena

            // recibir hops
            // si no quedan, se mandara ip = -1 y puerto 0
            // pero no se cierra la conexion
            uint32_t hops_net;
            if (recv(soc, &hops_net, sizeof(hops_net), MSG_WAITALL) != sizeof(hops_net)) {
                free(file_name);
                close(soc);
                return NULL;
            }

            uint32_t hops = ntohl(hops_net);


            // construir ruta completa del fichero
            char path[PATH_MAX];
            if (snprintf(path, sizeof(path), "%s/%s", g_srd_dir ? g_srd_dir : ".", file_name) >= (int)sizeof(path)) {
                free(file_name);
                close(soc);
                return NULL;
            }

            // buscar fichero localmente
            struct stat st;
            int found_locally = (stat(path, &st) == 0) ? 1 : 0;

            unsigned int resp_ip;
            unsigned short resp_port;

            if (found_locally) {
                // encontrado localmente: devolver IP/puerto
                resp_ip = g_local_ip;
                resp_port = g_local_port;
            } else if (hops == 0) {
                // no encontrado y no quedan hops: devolver error (-1)
                resp_ip = (unsigned int)(-1);
                resp_port = 0;
            } else {
                // no encontrado pero quedan hops: reenviar petición al sucesor
                if (ring_lookup(file_name, (int)hops, &resp_ip, &resp_port) < 0) {
                    resp_ip = (unsigned int)(-1);
                    resp_port = 0;
                }
            }

            // enviar respuesta (IP/port ya están en formato de red)
            uint32_t resp_ip_net = resp_ip;
            uint16_t resp_port_net = resp_port;
            if (send(soc, &resp_ip_net, sizeof(resp_ip_net), MSG_MORE) != sizeof(resp_ip_net)) {
                free(file_name);
                close(soc);
                return NULL;
            }
            if (send(soc, &resp_port_net, sizeof(resp_port_net), 0) != sizeof(resp_port_net)) {
                free(file_name);
                close(soc);
                return NULL;
            }
            free(file_name);
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