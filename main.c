/**
 * @author Alexandre Demelas
 * @link https://github.com/axdemelas/mini-http-server
 * @description A tiny HTTP Server with Winsock.
 * @license MIT
 */

// DEFINE SECTION

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define DEFAULT_PORT "3000"
#define DEFAULT_BUFLEN 20000
#define DEFAULT_SERVERROOT "C:\\webserver"
#define DEFAULT_MAXCLIENTS 30

// INCLUDE SECTION

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")

// INTERFACE SECTION

int initWinsock(WSADATA *wsaDataAddr);
void setAddrInfo(struct addrinfo *hints);
int defineServerInfo(struct addrinfo *hints, struct addrinfo *serverInfo);
int exitServer();
void log(char *message);
int getServerSocket(struct addrinfo serverInfo);
void logSockError(char *message);
int bindSocket(SOCKET ServerSocket, struct addrinfo serverInfo);
int listenSocket(SOCKET ServerSocket);
char *getHTTPRequestMethod(char *HTTPRequest);
char *getHTTPResponse(char *HTTPRequest);
char *processGetRequest(char *HTTPRequest);
char *getFileContent(char *filename);
char *resolvePathname(char *filename);
void initSocketDescriptors(struct fd_set *socketDescriptors, SOCKET ServerSocket, SOCKET ClientSocketList[]);
int acceptNewConnection(SOCKET ServerSocket, SOCKET ClientSocketList[]);
int getConnectedClientsCount(SOCKET ClientSocketList[]);
int processSocketActivity(SOCKET ClientSocketList[], int socketIndex);
void logSocket(SOCKET LogSocket, char *message);
void logHTTPMessage(char *title, char *content, SOCKET MessageSocket);

int main()
{
    WSADATA wsaData;
    struct addrinfo *serverInfo = NULL;
    struct addrinfo hints;
    SOCKET ServerSocket = INVALID_SOCKET;
    SOCKET ClientSocketList[DEFAULT_MAXCLIENTS];
    fd_set socketDescriptors;
    int socketActivity;
    int i;
    int success = 0;

    // Inicializa a lista de Sockets.
    for (i = 0; i < DEFAULT_MAXCLIENTS; i++) {
        ClientSocketList[i] = 0;
    }

    // Inicializa o winsock.
    if (!initWinsock(&(wsaData))) {
        log("Falha ao iniciar o Winsocket.");
        return 1;
    }

    // Especifica as informações de endereço que devem ser usadas no Socket servidor.
    setAddrInfo(&(hints));

    // Define as informações do Socket servidor (que escuta na porta e aceita conexoes)
    // com base na estrutura "hints" + porta.
    success = defineServerInfo(&hints, &serverInfo);

    if (!success) {
        log("Falha ao definir as informações do socket no servidor.");
        return exitServer();
    }

    // Cria o socket para o servidor com base nas informações definidas anteriormente.
    ServerSocket = getServerSocket(*serverInfo);

    // Verifica se o socket foi criado com sucesso.
    if (ServerSocket == INVALID_SOCKET) {
        logSockError("Falha ao criar o Socket servidor.");
        freeaddrinfo(serverInfo);
        return exitServer();
    }

    // Associa o Socket servidor ao endereco local.
    success = bindSocket(ServerSocket, *serverInfo);

    if (!success) {
        logSockError("Falha ao associar o Socket servidor a porta local.");
        freeaddrinfo(serverInfo);
        closesocket(ServerSocket);
        return exitServer();
    }

    // Remove a estrutura com informações do servidor da memoria.
    freeaddrinfo(serverInfo);

    // Habilita o Socket servidor a receber conexões com o cliente.
    success = listenSocket(ServerSocket);

    if (!success) {
        logSockError("Falha ao habilitar o Socket servidor para receber conexões.");
        closesocket(ServerSocket);
        return exitServer();
    }

    log("================================================");
    log("---------- Servidor em localhost:3000 ----------");
    log("================================================");

    while(1) {
        // Inicializa a estrutura para armazenar sockets.
        initSocketDescriptors(&(socketDescriptors), ServerSocket, ClientSocketList);

        // Espera alguma atividade nos sockets adicionados.
        // Parametro "timeout" definido como NULL (espera por tempo indeterminado).
        socketActivity = select(0, &(socketDescriptors), NULL, NULL, NULL);

        if (socketActivity == SOCKET_ERROR) {
            logSockError("Falha ao selecionar Sockets.");
            closesocket(ServerSocket);
            return exitServer();
        }

        // Verifica se existe atividade no Socket servidor.
        // Se existir, significa que algum cliente esta tentando se conectar.
        if (FD_ISSET(ServerSocket, &(socketDescriptors))) {

            // Tenta aceitar nova conexao.
            success = acceptNewConnection(ServerSocket, ClientSocketList);

            if (!success) {
                logSockError("Falha ao aceitar nova conexao.");
                closesocket(ServerSocket);
                return exitServer();
            }
        }

        // Verifica se existe atividade em outros Sockets conectados.
        // Se existir, significa que algum cliente esta enviando ou recebendo dados (I/O).
        for (i = 0; i < DEFAULT_MAXCLIENTS; i++) {

            if (FD_ISSET(ClientSocketList[i], &(socketDescriptors))) {
                // Processa a atividade do Socket.
                success = processSocketActivity(ClientSocketList, i);

                if (!success) {
                    closesocket(ClientSocketList[i]);
                    closesocket(ServerSocket);
                    return exitServer();
                }
            }
        }
    }

    WSACleanup();
    return 0;
}

/**
 * @brief initWinsock Tenta inicializar o Winsock.
 * @param wsaDataAddr
 * @return 1 (sucesso), 0 (falha).
 */
int initWinsock(WSADATA *wsaDataAddr)
{
    return (WSAStartup(MAKEWORD(2, 2), wsaDataAddr) == 0);
}

/**
 * @brief setAddrInfo
 * @param hints
 */
void setAddrInfo(struct addrinfo *hints)
{
    // Especifica as informaçoes preferidas para o Socket.
    ZeroMemory(hints, sizeof(*(hints)));
    hints->ai_family = AF_INET; // Indica o protocolo IPv4.
    hints->ai_socktype = SOCK_STREAM; // Indica servico orientado a conexao e entrega confiavel dos dados (full-duplex).
    hints->ai_protocol = IPPROTO_TCP; // Indica o protocolo TCP.
    hints->ai_flags = AI_PASSIVE; // Indica que o Socket e adequado para aceitar conexoes.
}

/**
 * @brief defineServerInfo
 * @param hints
 * @param serverInfo
 * @return 1 (sucesso), 0 (falha).
 */
int defineServerInfo(struct addrinfo *hints, struct addrinfo *serverInfo)
{
    return (getaddrinfo(NULL, DEFAULT_PORT, hints, serverInfo) == 0);
}

/**
 * @brief exitServer
 * @return 1
 */
int exitServer()
{
    WSACleanup();
    return 1;
}

/**
 * @brief log
 * @param message
 */
void log(char *message)
{
    printf("\n%s\n", message);
    fflush(stdout);
}

/**
 * @brief getServerSocket
 * @param serverInfo
 * @return Retorna o "file descriptor".
 */
int getServerSocket(struct addrinfo serverInfo)
{
    return socket(serverInfo.ai_family, serverInfo.ai_socktype, serverInfo.ai_protocol);
}

/**
 * @brief logSockError
 * @param message
 */
void logSockError(char *message)
{
    log(message);
    printf("\nErro de Socket: %d\n", WSAGetLastError());
    fflush(stdout);
}

/**
 * @brief bindSocket
 * @param ServerSocket
 * @param serverInfo
 * @return
 */
int bindSocket(SOCKET ServerSocket, struct addrinfo serverInfo)
{
    return (bind(ServerSocket, serverInfo.ai_addr, serverInfo.ai_addrlen) != SOCKET_ERROR);
}

/**
 * @brief listenSocket
 * @param ServerSocket
 * @return
 */
int listenSocket(SOCKET ServerSocket)
{
    return (listen(ServerSocket, SOMAXCONN) != SOCKET_ERROR);
}

/**
 * @brief getHTTPRequestMethod
 * @param HTTPRequest
 * @return
 */
char *getHTTPRequestMethod(char *HTTPRequest)
{
    if (strstr(HTTPRequest, "GET")) {
        return "GET";
    }

    return NULL;
}

/**
 * @brief getHTTPResponse
 * @param HTTPRequest
 * @return
 */
char *getHTTPResponse(char *HTTPRequest)
{
    char *requestMethod = getHTTPRequestMethod(HTTPRequest);
    char *HTTPResponse;

    if (requestMethod != NULL && strcmp(requestMethod, "GET") == 0) {
        // Atende requisição GET.
        HTTPResponse = processGetRequest(HTTPRequest);
    } else {
        // Metodo nao permitido.
        HTTPResponse = "HTTP/1.1 405 Method Not Allowed\n\n<h1>405 Method Not Allowed</h1>";
    }

    return HTTPResponse;
}

/**
 * @brief processGetRequest
 * @param HTTPRequest
 * @return
 */
char *processGetRequest(char *HTTPRequest)
{
    char *response;
    char *content;
    char *status;
    char HTTPVersion[] = "HTTP/1.1";
    char *startOfPath = strchr(HTTPRequest, '/');
    char *endOfPath = strchr(startOfPath, ' ');
    char path[endOfPath - startOfPath];
    strncpy(path, startOfPath, endOfPath - startOfPath);
    path[sizeof(path)] = 0;

    // Valor padrao do status da requisicao.
    status = "200 OK";

    // Tratando "/" e "/index.html" igualmente.
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        content = getFileContent("/index.html");
    } else if (strcmp(path, "/error.html") == 0)  {
        content = getFileContent(path);
        status = "500 Internal Server Error";
    } else {
        content = getFileContent(path);
    }

    // Verifica se o arquivo não foi encontrado no servidor.
    if (content == NULL) {
        // Busca a pagina "404.html".
        content = getFileContent("/404.html");

        // Verifica se a pagina "404.html" nao foi encontrada.
        if (content == NULL) {
            content = "<h1>404 Not Found</h1>";
        }
        status = "404 Not Found";
    }

    response = (char *) malloc((strlen(HTTPVersion) + strlen(status) + strlen(content) + 1) * sizeof(char));

    strcpy(response, HTTPVersion);
    strcat(response, " ");
    strcat(response, status);
    strcat(response, "\n\n");
    strcat(response, content);

    free(content);

    return response;
}

/**
 * @brief getFileContent
 * @param filename
 * @return
 */
char *getFileContent(char *filename)
{
    FILE *file;
    char *fileContent;
    long fileSize;
    char *pathname = resolvePathname(filename);

    file = fopen(pathname, "rb");
    free(pathname);

    if (file != NULL) {
        fseek(file, 0, SEEK_END);
        fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        fileContent = (char *) malloc((fileSize + 1) * (sizeof(char)));
        fread(fileContent, sizeof(char), fileSize, file);
        fclose(file);
        fileContent[fileSize] = 0;
        return fileContent;
    }

    return NULL;
}

/**
 * @brief resolvePathname
 * @param filename
 * @return
 */
char *resolvePathname(char *filename)
{
    char serverRoot[] = DEFAULT_SERVERROOT;
    char resolvedFilename[(strlen(filename) * 2)];
    const char *pathname;
    int i = 0;

    for (i; filename[i] != '\0'; i++) {

        if (filename[i] == '/') {
            resolvedFilename[i] = '\\';
        } else {
            resolvedFilename[i] = filename[i];
        }
    }
    resolvedFilename[i] = '\0';

    pathname = (char *) malloc(((strlen(serverRoot) + strlen(resolvedFilename)) * 2) * sizeof(char));

    strcpy(pathname, serverRoot);
    strcat(pathname, resolvedFilename);

    return pathname;
}

/**
 * @brief initSocketDescriptors
 * @param socketDescriptors
 * @param ServerSocket
 * @param ClientSocketList
 */
void initSocketDescriptors(fd_set *socketDescriptors, SOCKET ServerSocket, SOCKET ClientSocketList[])
{
    // Inicializa a estrutura para armazenar Sockets.
    FD_ZERO(socketDescriptors);
    // Adiciona o Socket do servidor.
    FD_SET(ServerSocket, socketDescriptors);
    int i;

    // Adiciona os Sockets clientes.
    for (i = 0; i < DEFAULT_MAXCLIENTS; i++) {

        if (ClientSocketList[i] > 0) {
            FD_SET(ClientSocketList[i], socketDescriptors);
        }
    }
}

/**
 * @brief acceptNewConnection
 * @param ServerSocket
 * @param ClientSocketList
 * @return
 */
int acceptNewConnection(SOCKET ServerSocket, SOCKET ClientSocketList[])
{
    SOCKET RecentSocket = 0;
    int i;

    // Aceita uma nova conexao ao Socket servidor.
    if ((RecentSocket = accept(ServerSocket, NULL, NULL)) == INVALID_SOCKET) {
        return 0;
    }

    // Adiciona o novo Socket conectado a lista de Socket clientes.
    for (i = 0; i < DEFAULT_MAXCLIENTS; i++) {

        if (ClientSocketList[i] == 0) {
            ClientSocketList[i] = RecentSocket;
            break;
        }
    }

    logSocket(RecentSocket, "Se conectou.");
    log("+--------------------------------+");
    printf("+---- clientes conectados: %d ----+", getConnectedClientsCount(ClientSocketList));
    log("+--------------------------------+");
    return 1;
}

/**
 * @brief getConnectedClientsCount
 * @param ClientSocketList
 * @return
 */
int getConnectedClientsCount(SOCKET ClientSocketList[])
{
    int i;
    int count = 0;

    for (i = 0; i < DEFAULT_MAXCLIENTS; i++) {
        if (ClientSocketList[i] != 0) count++;
    }

    return count;
}

/**
 * @brief processSocketActivity
 * @param ClientSocketItem
 * @param ClientSocketList
 * @return
 */
int processSocketActivity(SOCKET ClientSocketList[], int socketIndex)
{
    char *HTTPRequest;
    char *HTTPResponse;
    int success = 0;
    SOCKET ClientSocketItem = ClientSocketList[socketIndex];
    HTTPRequest = (char *) malloc((DEFAULT_BUFLEN + 1) * sizeof(char));

    // Recebendo dados (requisicao HTTP) do cliente.
    success = recv(ClientSocketItem, HTTPRequest, DEFAULT_BUFLEN, 0);

    // Verifica se houve erro ao receber dados.
    if (success == SOCKET_ERROR) {

        if (WSAGetLastError() == WSAECONNRESET) {
            logSocket(ClientSocketItem, "Desconectou inesperadamente.");
        } else {
            logSockError("Falha ao receber dados do cliente.");
        }
    }

    // Se nao houve erro, o cliente foi desconectado ou esta esperando uma resposta.
    if (success == 0) {
        logSocket(ClientSocketItem, "Desconectado.");
    } else {
        logHTTPMessage("HTTP Request", HTTPRequest, ClientSocketItem);

        HTTPResponse = getHTTPResponse(HTTPRequest);

        if (send(ClientSocketItem, HTTPResponse, strlen(HTTPResponse), 0) == SOCKET_ERROR) {
            logSockError("Falha ao enviar a resposta HTTP.");
            return 0;
        }

        if (shutdown(ClientSocketItem, SD_SEND) == SOCKET_ERROR) {
            logSockError("Falha ao desligar o Socket cliente.");
            return 0;
        }

        logHTTPMessage("HTTP Response", HTTPResponse, ClientSocketItem);
        free(HTTPResponse);
        logSocket(ClientSocketItem, "Atendido. Fechando a conexao...");
    }

    closesocket(ClientSocketItem);
    ClientSocketList[socketIndex] = 0;
    return 1;
}

/**
 * @brief logSocket
 * @param LogSocket
 * @param message
 */
void logSocket(SOCKET LogSocket, char *message)
{
    printf("\n\n+- SOCKET: %d", LogSocket);
    printf("\n+- Mensagem: %s\n", message);
}

/**
 * @brief logHTTPMessage
 * @param title
 * @param content
 * @param MessageSocket
 */
void logHTTPMessage(char *title, char *content, SOCKET MessageSocket)
{
    printf("\n======== %s =======\n", title);
    printf("\n+- SOCKET: %d", MessageSocket);
    printf("\n+- content: %s", content);
    printf("\n======== eof %s ===\n", title);
}
