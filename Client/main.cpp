#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <iostream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

int main(int argc, char *argv[])
{
    WSADATA wsaData;
    int iResult;

    if (argc != 2)
    {
        printf("Specify server IP\n");
        printf("Client.exe <IP>\n");
        return 1;
    }

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0) 
    {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    SOCKET ConnectSocket = INVALID_SOCKET;
    // Attempt to connect to the first address returned by
    // the call to getaddrinfo
    ptr = result;

    // Create a SOCKET for connecting to server
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) 
    {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Connect to server.
    iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) 
    {
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
    }

    // Should really try the next address returned by getaddrinfo
    // if the connect call failed
    // But for this simple example we just free the resources
    // returned by getaddrinfo and print an error message

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) 
    {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }
    
    bool bCloseConnection = false;
    printf("Client is now ready to communicate with server\n");
    while (1)
    {
        int recvbuflen = DEFAULT_BUFLEN;
        char recvbuf[DEFAULT_BUFLEN]{};
        std::cin >> recvbuf;
        int dataLen = strlen(recvbuf);
        int iSendResult = send(ConnectSocket, recvbuf, dataLen, 0);
        if (iSendResult == SOCKET_ERROR)
        {
            printf("send failed: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
        }

        int iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
        {
            //printf("Bytes received: %d\n", iResult);
            printf("%s\n", recvbuf);
            if (strcmp(recvbuf, "CLOSE_CONNECTION") == 0)
            {
                bCloseConnection = true;
                break;
            }
        }
        else if (iResult == 0)
        {
            printf("Connection closed\n");
            bCloseConnection = true;
            break;
            
        }
        else
        {
            printf("recv failed: %d\n", WSAGetLastError());
            bCloseConnection = true;
            break;
        }
    }// End of while loop


    if (bCloseConnection)
    {
        // shutdown the send half of the connection since no more data will be sent
        iResult = shutdown(ConnectSocket, SD_SEND);

        // cleanup
        closesocket(ConnectSocket);
        WSACleanup();
    }


    return 0;
}