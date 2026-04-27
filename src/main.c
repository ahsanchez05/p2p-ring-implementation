#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "ring.h"

static void user_commands(void);
static unsigned int getIP(const char *host, unsigned int *ip);
static char * getIPdot(unsigned int ip);
static unsigned int get_host_info(char *hostname, size_t lon, unsigned int *ip);

int main(int argc, char *argv[]) {
    if ((argc!=4) && (argc!=2)) {
        fprintf(stderr, "Usage: %s shared_dir [remote_host remote_port]\n", argv[0]);
        return -1;
    }
    // validando directorio
    char *shared_dir=argv[1];
    struct stat st;
    if ((stat(shared_dir, &st) == -1) || (!S_ISDIR(st.st_mode))) {
        fprintf(stderr, "error en directorio compartido\n"); return -1;
    }
    char local_host[256];
    unsigned int local_ip;
    unsigned short local_port = 0;
    unsigned int remote_ip = 0;
    unsigned short remote_port = 0;

    // obteniendo nombre de host e IP del equipo local
    if (get_host_info(local_host, sizeof(local_host), &local_ip) < 0) {
        fprintf(stderr, "error: obteniendo información del host\n"); return -1;
    }
    // recogiendo y validando el resto de parámetros
    if (argc==4) { // no es el primer nodo del anillo
        if (getIP(argv[2], &remote_ip) < 0) {
            fprintf(stderr, "error en especificación de equipo remoto\n"); return -1;
	}
        remote_port=htons(atoi(argv[3]));
    }
    // debe devolver en el último parámetro el puerto local reservado
    if ((ring_init(shared_dir, local_ip, remote_ip, remote_port, &local_port) < 0) || (!local_port)) {
        fprintf(stderr, "error: ring_init\n"); return 1;
    }
    // mensaje de bienvenida
    printf("Bienvenido a la red P2P RING\n");
    printf("----------------------------\n");
    printf("El equipo %s con IP %s y puerto %d (PID %d) ", local_host, getIPdot(local_ip), ntohs(local_port), getpid());
    if (remote_ip)
         printf("se incorpora a la red a través del equipo %s puerto %d\n", getIPdot(remote_ip), ntohs(remote_port));
    else
         printf("es el primer nodo de una nueva red\n");

    user_commands();
    return 0;
}

static char * leer_string(char *prompt) {
    int n;
    char *str;
    char *lin=NULL;
    size_t ll=0;
    fputs(prompt, stdout);
    n=getline(&lin, &ll, stdin);
    if (n<1) {ungetc(' ', stdin); return NULL;}
    n=sscanf(lin, "%ms", &str);
    free(lin);
    if (n!=1) return NULL;
    return str;
}
int leer_int(char *prompt) {
    int n, v;
    char *lin=NULL;
    size_t ll=0;
    fputs(prompt, stdout);
    n=getline(&lin, &ll, stdin);
    if (n<1) {ungetc(' ', stdin); return -1;}
    n=sscanf(lin, "%d", &v);
    free(lin);
    if (n!=1) return -1;
    return v;
}

static void user_commands(void) {
    char *op;
    char *host;
    char *fichero;
    int hops;
    while (1) {
        op=leer_string("\nSeleccione operación (línea vacía para terminar; en menús internos para volver a menú principal)\n\tI: obtiene Info de nodo local| P: getPid|S: Sucesor|R:sucesor Remoto|U: sUcesor de sucesor remoto|D: Download|L: Lookup fichero|G: Get fichero (lookup+download)\n");
        if (op==NULL) break;
        switch(op[0]) {
            case 'I':
		unsigned int ip;
		unsigned short port;
                ring_self(&ip, &port);
                printf("\nIP %s port %d\n", getIPdot(ip), ntohs(port));
                break;
            case 'P':
                host=leer_string("Introduzca el nombre o la IP del host remoto: ");
                if (host==NULL) continue;
                if (getIP(host, &ip) < 0) continue;
                port=leer_int("Introduzca el puerto del host remoto: ");
                if (port==-1) continue;
                int pid;
                if ((pid=ring_remote_pid(ip, htons(port)))<0)
                    printf("error en ring_remote_pid\n");
                else {
                    printf("\nPID %d\n", pid);
                }
                break;
            case 'S':
                ring_successor(&ip, &port);
                printf("\nIP %s port %d\n", getIPdot(ip), ntohs(port));
                break;
            case 'R':
                host=leer_string("Introduzca el nombre o la IP del host remoto: ");
                if (host==NULL) continue;
                if (getIP(host, &ip) < 0) continue;
                port=leer_int("Introduzca el puerto del host remoto: ");
                if (port==-1) continue;
		unsigned int suc_ip;
		unsigned short suc_port;
                if (ring_remote_successor(ip, htons(port), &suc_ip, &suc_port)<0)
                    printf("error en ring_remote_successor\n");
                else {
                    printf("\nIP %s port %d\n", getIPdot(suc_ip), ntohs(suc_port));
                }
                break;
            case 'U':
                host=leer_string("Introduzca el nombre o la IP del host remoto: ");
                if (host==NULL) continue;
                if (getIP(host, &ip) < 0) continue;
                port=leer_int("Introduzca el puerto del host remoto: ");
                if (port==-1) continue;
		unsigned int suc_suc_ip;
		unsigned short suc_suc_port;
                if (ring_remote_successor_successor(ip, htons(port), &suc_suc_ip, &suc_suc_port)<0)
                    printf("error en ring_get_suc_suc\n");
                else {
                    printf("\nIP %s port %d\n", getIPdot(suc_suc_ip), ntohs(suc_suc_port));
                }
                break;
            case 'D':
                host=leer_string("Introduzca el nombre o la IP del host remoto: ");
                if (host==NULL) continue;
                if (getIP(host, &ip) < 0) continue;
                port=leer_int("Introduzca el puerto del host remoto: ");
                if (port==-1) continue;
                fichero=leer_string("Introduzca el nombre del fichero: ");
                if (fichero==NULL) continue;
                if (ring_download(ip, htons(port), fichero)<0)
                    printf("error en ring_download\n");
                break;
            case 'L':
                fichero=leer_string("Introduzca el nombre del fichero: ");
                if (fichero==NULL) continue;
                hops=leer_int("Introduzca el número máximo de nodos visitados: ");
                if (hops==-1) continue;
                if (ring_lookup(fichero, hops, &ip, &port)<0)
                    printf("error en ring_lookup\n");
                else {
                    printf("\nIP %s port %d\n", getIPdot(ip), ntohs(port));
                }
                break;
            case 'G':
                fichero=leer_string("Introduzca el nombre del fichero: ");
                if (fichero==NULL) continue;
                hops=leer_int("Introduzca el número máximo de nodos visitados: ");
                if (hops==-1) continue;
                if (ring_get_file(fichero, hops)<0)
                    printf("error en ring_get_file\n");
                break;
            default:
                printf("operación no válida\n");
        }
        if (op) free(op);
    }
}
static unsigned int getIP(const char *host, unsigned int *ip) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;    /* solo IPv4 */

    struct addrinfo *res;
    // obtiene la dirección IP
    if (getaddrinfo(host, NULL, &hints, &res)!=0) {
        perror("error en getaddrinfo"); return -1;
    }
    if (ip) *ip = ((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr;
    freeaddrinfo(res);
    return 0;
}
static char * getIPdot(unsigned int ip) {
    return inet_ntoa((struct in_addr){ip});
}

// Retorna nombre del equipo y su IPv4
unsigned int get_host_info(char *hostname, size_t lon, unsigned int *ip) {
    if (!hostname) return -1;
    if (gethostname(hostname, lon)<0) return -1;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;    /* solo IPv4 */

    struct addrinfo *res;
    // obtiene la dirección IP
    if (getaddrinfo(hostname, NULL, &hints, &res)!=0) {
        perror("error en getaddrinfo"); return -1;
    }
    if (ip) *ip=((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr;
    freeaddrinfo(res);
    return 0;
}

