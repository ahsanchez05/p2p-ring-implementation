// DECLARACIÓN DE LA FUNCIONALIDAD COMÚN PARA LA PARTE CLIENTE
// Y LA PARTE SERVIDORA
#ifndef _COMMON_H
#define _COMMON_H        1

// thread de servicio
void *server_thread(void *arg);

// crea un thread detached
int create_thread(void *(*func)(void *), void *arg);

// Inicializa el socket, le asigna un puerto seleccionado por el SO
// y lo prepara para aceptar conexiones. Recibe un parámetro de salida
// donde devuelve el puerto asignado.
int create_socket_srv(unsigned short *port);

// crea socket y se conecta al servidor a partir de IP y puerto en formato red
int create_socket_cln(unsigned int ip, unsigned short port);

#endif // _COMMON_H
