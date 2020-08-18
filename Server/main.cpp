#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

typedef struct ConnectionInfo_tag
{
    int id;
    SOCKET clientSocket;
}ConnectionInfo_t;

// Download Information
static int g_downloadProgress = 0;
static int g_totalFiles = 0;
static int g_filesDownloaded = 0;


void Downloader()
{
    std::mutex mtx;

    // Genereate a random seed
    srand(time(nullptr));

    mtx.lock();
    g_totalFiles = rand() % 100;
    mtx.unlock();

    for (int fileIndex = 0; fileIndex < g_totalFiles; fileIndex++)
    {
        for (int downloadProgress = 0; downloadProgress < 100; downloadProgress++)
        {
            mtx.lock();
            g_downloadProgress = downloadProgress;
            mtx.unlock();
            Sleep(rand() % 1000);
        }

        mtx.lock();
        g_filesDownloaded++;
        mtx.unlock();
    }
}

bool ProcessClientRequest(ConnectionInfo_t connectionInfo, const std::string & msg)
{
    bool bCloseClient = false;
    char response[DEFAULT_BUFLEN]{};

    if (msg == "GET_STATUS")
    {
        if (g_filesDownloaded == g_totalFiles)
        {
            sprintf(response, "Download complete for %d files", g_filesDownloaded);
        }
        else
        {
            sprintf(response, "Downloading file [%d/%d]. Progress %d%%", g_filesDownloaded + 1, g_totalFiles, g_downloadProgress);
        }
    }
    else if (msg == "CLOSE" || msg == "x")
    {
        bCloseClient = true;
        strcpy(response, "CLOSE_CONNECTION");
    }
    else
    {
        strcpy(response, "NOT A VALID QUERY");
    }


    int iSendResult = send(connectionInfo.clientSocket, response, strlen(response), 0);
    if (iSendResult == SOCKET_ERROR)
    {
        printf("send failed: %d\n", WSAGetLastError());
        closesocket(connectionInfo.clientSocket);
        WSACleanup();
    }

    printf("Bytes sent: %d\n", iSendResult);

    return bCloseClient;
}


void ServeClient(ConnectionInfo_t connectionInfo)
{
    bool bCloseClient = false;
    int iResult = 0;
    int iSendResult = 0;
    printf("Processing client #%d\n", connectionInfo.id);

    while (1)
    {
        int recvbuflen = DEFAULT_BUFLEN;
        char recvbuf[DEFAULT_BUFLEN]{};
        int iResult = recv(connectionInfo.clientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
        {
            printf("Bytes received: %d\n", iResult);
            printf("Data: %s\n", recvbuf);
            std::string msg(recvbuf);
            bCloseClient = ProcessClientRequest(connectionInfo, msg);
        }
        else if (iResult == 0)
        {
            printf("Connection closed\n");
            bCloseClient = true;
            break;
        }
        else
        {
            printf("recv failed: %d\n", WSAGetLastError());
            break;
        }
    }// End of while loop

    if (bCloseClient)
    {
        // shutdown the send half of the connection since no more data will be sent
        iResult = shutdown(connectionInfo.clientSocket, SD_SEND);

        // cleanup
        closesocket(connectionInfo.clientSocket);        

        printf("Closing connection to %d...\n", connectionInfo.id);
    }
}


int GetIpFromSocket(SOCKET socket, char *ipstr, int bufSize)
{
    struct sockaddr_storage addr;
    int len = sizeof(struct sockaddr);
    int status = getpeername(socket, (struct sockaddr*) & addr, &len);
    struct sockaddr_in* s = (struct sockaddr_in*) & addr;
    inet_ntop(AF_INET, &s->sin_addr, ipstr, bufSize);
    return status;
}


int main() 
{
    std::thread downloaderThread(Downloader);
    downloaderThread.detach();

    WSADATA wsaData;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) 
    {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;
    struct sockaddr_in  clientAddr {};

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the local address and port to be used by the server
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) 
    {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections
    SOCKET listenSocket = INVALID_SOCKET;
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSocket == INVALID_SOCKET)
    {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) 
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        printf("Listen failed with error: %ld\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    int clientsServed = 0;
    printf("Waiting for connection...\n");

    while (1)
    {
        SOCKET clientSocket;
        clientSocket = INVALID_SOCKET;

        // Accept a client socket      
        clientSocket = accept(listenSocket, NULL, NULL);
        //clientSocket = accept(listenSocket, &clientAddr, &clientAddrLen);        
        if (clientSocket != INVALID_SOCKET)
        {
            char ipstr[INET6_ADDRSTRLEN];
            GetIpFromSocket(clientSocket, ipstr, sizeof (ipstr));
            printf("\n*** Connected to [ %s ] ***\n", ipstr);

            ConnectionInfo_t connectionInfo;
            connectionInfo.id = ++clientsServed;
            connectionInfo.clientSocket = clientSocket;
            std::thread clientProcessingThread(ServeClient, connectionInfo);
            clientProcessingThread.detach();
        }
    }// End of while loop

    WSACleanup();

    return 0;
}