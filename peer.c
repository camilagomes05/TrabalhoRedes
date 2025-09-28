// ========================================================================
//                     INCLUSÃO DE BIBLIOTECAS (CABEÇALHOS)
// ========================================================================
#include <stdio.h>      // Para funções de entrada e saída padrão (printf, snprintf)
#include <stdlib.h>     // Para funções gerais como alocação de memória (malloc) e conversão de strings (atoi)
#include <string.h>     // Para manipulação de strings (strcpy, strcmp, strtok, memset, memcpy)
#include <unistd.h>     // Para chamadas de sistema POSIX como sleep(), close(), access(), remove()
#include <pthread.h>    // Para a criação e gerenciamento de threads (POSIX Threads)
#include <sys/socket.h> // Para a API de sockets (socket, bind, listen, accept, connect, send, recv)
#include <netinet/in.h> // Para as estruturas de endereço da internet (struct sockaddr_in)
#include <arpa/inet.h>  // Para funções de manipulação de endereços IP
#include <dirent.h>     // Para manipulação de diretórios (opendir, readdir, closedir)
#include <sys/stat.h>   // Para obter informações de arquivos e diretórios (stat, mkdir)
#include <time.h>       // Funções relacionadas a tempo
#include <netdb.h>      // Biblioteca para resolução de nomes (DNS), essencial para o Docker (gethostbyname)

// ========================================================================
//                             DEFINIÇÕES GLOBAIS
// ========================================================================
#define MAX_PEERS 10             // Número máximo de outros peers que este nó pode conhecer
#define BUFFER_SIZE 4096         // Tamanho do buffer para leitura/escrita de dados de rede e arquivos (4KB)
#define SYNC_DIR "tmp"           // Nome do diretório que será sincronizado
#define SYNC_INTERVAL 15         // Intervalo em segundos para a sincronização periódica ("pull")

// Estrutura para representar um peer na rede
typedef struct {
    char ip[256]; // Endereço do peer (IP ou nome do host, como "peer2" no Docker)
    int port;     // Porta em que o peer está escutando por conexões
} Peer;

// Variáveis Globais, acessíveis por todas as threads
Peer self;                                    // Estrutura para guardar as informações do próprio peer
Peer known_peers[MAX_PEERS];                  // Array para armazenar os outros peers conhecidos na rede
int peer_count = 0;                           // Contador de quantos peers conhecidos existem
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER; // Mutex para garantir acesso seguro aos arquivos no disco

// Protótipo da função (declaração antecipada) para que outras funções possam chamá-la
void request_file(const Peer* target, const char* filename);

// ========================================================================
//                    LÓGICA DE REDE (FUNÇÕES "CLIENTE")
// ========================================================================
static int connect_to_peer(const Peer* target) {
    int sockfd;
    struct hostent *server;
    struct sockaddr_in serv_addr;

    server = gethostbyname(target->ip);
    if (server == NULL) {
        fprintf(stderr, "[-] ERRO, host não encontrado: %s\n", target->ip);
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(target->port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void request_file(const Peer* target, const char* filename) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", SYNC_DIR, filename);

    pthread_mutex_lock(&file_lock);
    if (access(filepath, F_OK) == 0) {
        pthread_mutex_unlock(&file_lock);
        return;
    }
    pthread_mutex_unlock(&file_lock);

    printf("[>] Solicitando arquivo '%s' de %s:%d\n", filename, target->ip, target->port);
    
    int sockfd = connect_to_peer(target);
    if (sockfd < 0) return;
    
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "GET %s\n", filename);
    send(sockfd, buffer, strlen(buffer), 0);

    FILE* fp = fopen(filepath, "wb");
    if (fp == NULL) {
        close(sockfd);
        return;
    }
    int bytes_received;
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, fp);
    }
    fclose(fp);
    close(sockfd);
    printf("[+] Arquivo '%s' recebido com sucesso.\n", filename);
}

static void broadcast_notification(const char* message) {
    for (int i = 0; i < peer_count; i++) {
        int sockfd = connect_to_peer(&known_peers[i]);
        if (sockfd > 0) {
            send(sockfd, message, strlen(message), 0);
            close(sockfd);
        }
    }
}

// ========================================================================
//                THREADS DE CONTROLE (O CORAÇÃO AUTOMÁTICO)
// ========================================================================
void* monitor_directory_thread_func(void* arg) {
    char known_files[512][256] = {0};
    int known_file_count = 0;

    DIR *d = opendir(SYNC_DIR);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) strncpy(known_files[known_file_count++], dir->d_name, 255);
        }
        closedir(d);
    }

    while(1) {
        sleep(5);

        // --- Lógica de ADIÇÃO ---
        d = opendir(SYNC_DIR);
        if (d) {
            struct dirent *dir;
            while ((dir = readdir(d)) != NULL) {
                if (dir->d_type == DT_REG) {
                    int found = 0;
                    for (int i = 0; i < known_file_count; i++) if (strcmp(known_files[i], dir->d_name) == 0) found = 1;
                    
                    if (!found) {
                         printf("[!] Novo arquivo local detectado: %s. Notificando a rede...\n", dir->d_name);
                         strncpy(known_files[known_file_count++], dir->d_name, 255);
                         
                         char msg[512];
                         snprintf(msg, sizeof(msg), "NOTIFY_ADD %s\n", dir->d_name);
                         broadcast_notification(msg);
                    }
                }
            }
            closedir(d);
        }
        
        // --- Lógica de REMOÇÃO ---
        for (int i = 0; i < known_file_count; i++) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", SYNC_DIR, known_files[i]);
            
            if (access(filepath, F_OK) != 0) {
                printf("[!] Arquivo local removido: %s. Notificando a rede...\n", known_files[i]);
                
                char msg[512];
                snprintf(msg, sizeof(msg), "NOTIFY_DEL %s\n", known_files[i]);
                broadcast_notification(msg);
                
                for(int j = i; j < known_file_count - 1; j++) {
                    strcpy(known_files[j], known_files[j+1]);
                }
                known_file_count--;
                i--;
            }
        }
    }
    return NULL;
}

void* sync_thread_func(void* arg) {
    while (1) {
        sleep(SYNC_INTERVAL);
        printf("\n--- Iniciando ciclo de sincronização periódica ---\n");
        for (int i = 0; i < peer_count; ++i) {
            int sockfd = connect_to_peer(&known_peers[i]);
            if (sockfd < 0) continue;

            send(sockfd, "LIST\n", 5, 0);
            char list_buffer[BUFFER_SIZE * 4] = {0}, temp_buffer[BUFFER_SIZE];
            int bytes_read;
            
            while((bytes_read = recv(sockfd, temp_buffer, BUFFER_SIZE - 1, 0)) > 0) {
                temp_buffer[bytes_read] = '\0';
                strncat(list_buffer, temp_buffer, sizeof(list_buffer) - strlen(list_buffer) - 1);
            }
            close(sockfd);
            
            char* filename = strtok(list_buffer, "\n");
            while(filename != NULL) {
                if (strlen(filename) > 0) request_file(&known_peers[i], filename);
                filename = strtok(NULL, "\n");
            }
        }
        printf("--- Fim do ciclo de sincronização ---\n");
        fflush(stdout);
    }
    return NULL;
}

// ========================================================================
//                   LÓGICA DO SERVIDOR (RESPONDENDO A PEDIDOS)
// ========================================================================
void* handle_connection(void* socket_desc) {
    int client_sock = *(int*)socket_desc;
    free(socket_desc);
    char buffer[BUFFER_SIZE] = {0};
    read(client_sock, buffer, sizeof(buffer) - 1);
    buffer[strcspn(buffer, "\r\n")] = 0;

    if (strncmp(buffer, "LIST", 4) == 0) {
        DIR *d = opendir(SYNC_DIR);
        if (d) {
            struct dirent *dir;
            while ((dir = readdir(d)) != NULL) {
                if (dir->d_type == DT_REG) {
                    snprintf(buffer, sizeof(buffer), "%s\n", dir->d_name);
                    send(client_sock, buffer, strlen(buffer), 0);
                }
            }
            closedir(d);
        }
    } else if (strncmp(buffer, "GET ", 4) == 0) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", SYNC_DIR, buffer + 4);
        FILE* fp = fopen(filepath, "rb");
        if (fp) {
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
                send(client_sock, buffer, bytes_read, 0);
            }
            fclose(fp);
        }
    } else if (strncmp(buffer, "NOTIFY_ADD ", 11) == 0) {
        char* filename = buffer + 11;
        printf("[i] Notificação recebida: novo arquivo '%s'. Iniciando download...\n", filename);
        for (int i = 0; i < peer_count; i++) {
            request_file(&known_peers[i], filename);
        }
    } else if (strncmp(buffer, "NOTIFY_DEL ", 11) == 0) {
        char* filename = buffer + 11;
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", SYNC_DIR, filename);

        pthread_mutex_lock(&file_lock);
        if (remove(filepath) == 0) {
            printf("[i] Notificação recebida: arquivo '%s' removido localmente.\n", filename);
        }
        pthread_mutex_unlock(&file_lock);
    }
    
    close(client_sock);
    return NULL;
}

void* server_thread_func(void* arg) {
    int server_fd, client_sock;
    struct sockaddr_in server_addr;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(self.port);
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);
    printf("[+] Servidor escutando na porta %d\n", self.port);
    while ((client_sock = accept(server_fd, NULL, NULL))) {
        pthread_t handler_thread;
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;
        pthread_create(&handler_thread, NULL, handle_connection, (void*)new_sock);
    }
    return NULL;
}

// ========================================================================
//                             FUNÇÃO PRINCIPAL
// ========================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <porta_local> [ip_peer1:porta1] ...\n", argv[0]);
        return 1;
    }
    self.port = atoi(argv[1]);

    for (int i = 2; i < argc; i++) {
        char* token = strtok(argv[i], ":");
        if (token) strncpy(known_peers[peer_count].ip, token, 255);
        token = strtok(NULL, ":");
        if (token) known_peers[peer_count].port = atoi(token);
        peer_count++;
    }

    printf("[+] Peer inicializado. Escutando na porta %d\n", self.port);
    if (peer_count > 0) printf("[i] Conectando a %d outros peers.\n", peer_count);

    struct stat st = {0};
    if (stat(SYNC_DIR, &st) == -1) mkdir(SYNC_DIR, 0700);

    pthread_t server_tid, sync_tid, monitor_tid;
    pthread_create(&server_tid, NULL, server_thread_func, NULL);
    pthread_create(&sync_tid, NULL, sync_thread_func, NULL);
    pthread_create(&monitor_tid, NULL, monitor_directory_thread_func, NULL);

    pthread_join(server_tid, NULL);
    pthread_join(sync_tid, NULL);
    pthread_join(monitor_tid, NULL);

    return 0;
}