//
// Created by Leshi Chen on 2022/11/28.
//
#include <thread>
#include <cstring>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

#define CLIENT_LOGIN_IN_REQUEST 1
#define SERVER_LOGIN_IN_RESPONSE_SUCCESS 2
#define SERVER_LOGIN_IN_RESPONSE_USERNAME_ERROR 3
#define SERVER_LOGIN_IN_RESPONSE_PASSWORD_ERROR 4
#define CLIENT_QUERY_UNI_REQUEST 5
#define CLIENT_QUERY_MULTI_REQUEST 6
#define CLIENT_QUERY_REQUEST_CS 7
#define CLIENT_QUERY_REQUEST_EE 8
#define SERVER_QUERY_RESPONSE_SUCCESS 9
#define SERVER_QUERY_RESPONSE_FAIL 10

struct messageLogin{
    char username[50];
    char password[50];
};

struct messageQueryRequest{
    char username[50];
    int department;
    char courseCode[20];
    char category[20];
};

struct messageQueryMultiResponse{
    int result;
    char courseCode[10];
    char content[120];
};

class ChildTcpSocketInterface{
public:
    virtual void createChildTcpSocket(int socketID, sockaddr_in clientAddr){}
};

class ServerChildTcpSocket: ChildTcpSocketInterface{
private:
    int socketID;
    sockaddr_in clientAddr;

public:
    ServerChildTcpSocket(int socketID, sockaddr_in clientAddr) {
        this->socketID = socketID;
        this->clientAddr = clientAddr;
    }

    void createChildTcpSocket(int socketID, sockaddr_in clientAddr) override {
        this->socketID = socketID;
        this->clientAddr = clientAddr;
    }

    int getSocketID() {
        return socketID;
    }
};

class ServerParentTcpSocket{
private:
    string localIpAddr;
    int localPort;
    int tcpListenQueue;
    sockaddr_in serverAddr;
    int socketID;
    ServerChildTcpSocket* childTCPSocket;

public:
    ServerParentTcpSocket(string ipAddr, int portNum, int tcpListenQueue) {
        this->localIpAddr = localIpAddr;
        this->localPort = localPort;
        this->tcpListenQueue = tcpListenQueue;
        createServerTcpSocket();
        bindServerTcpSocket();
        listenTcpClient();
        acceptTcpClient();
    }

    void operator()(){
        while(true){
            sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            int childTCPSocketID = ::accept(socketID, (struct sockaddr *) &clientAddr, &clientAddrLen);
            if (childTCPSocketID == -1) {
                perror("The parent TCP socket of server failed to accept. Please try again.\n");
                exit(0);
            }
            childTCPSocket->createChildTcpSocket(childTCPSocketID, clientAddr);
        }
    }

    void createServerTcpSocket() {
        //create TCP socket
        socketID = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socketID == -1) {
            perror("The parent TCP socket of server failed to create. Please try again.\n");
            exit(0);
        }
    }

    void bindServerTcpSocket() {
        //initialize IP address, port number
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverAddr.sin_port = htons(25410);

        //bind TCP socket to the IP address and the port number
        if (::bind(socketID, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1) {
            perror("The parent TCP socket of server failed to bind. Please try again.\n");
            exit(0);
        }
    }

    void listenTcpClient() {
        if (listen(socketID, tcpListenQueue) == -1) {
            perror("The parent TCP socket of server failed to listen. Please try again.\n");
            exit(0);
        }
    }

    void acceptTcpClient() {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int childTCPSocketID = ::accept(socketID, (struct sockaddr *) &clientAddr, &clientAddrLen);
        if (childTCPSocketID == -1) {
            perror("The parent TCP socket of server failed to accept. Please try again.\n");
            exit(0);
        }
        childTCPSocket = new ServerChildTcpSocket(childTCPSocketID, clientAddr);
    }

    int getSocketID() {
        return socketID;
    }

    ServerChildTcpSocket* getServerChildTcpSocket(){
        return childTCPSocket;
    }
};

class ClientUdpSocket{
private:
    sockaddr_in serverAddr;
    int socketID;

public:
    ClientUdpSocket(string serverIpAddr, int serverPort) {
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(serverIpAddr.c_str());
        serverAddr.sin_port = htons(serverPort);
        createSocket();
    }

    void createSocket() {
        //create UDP socket
        socketID = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketID == -1) {
            perror("The UDP socket of server failed to create.\n");
            exit(0);
        }
    }

    int getSocketID() {
        return socketID;
    }
};

class UdpInterface{
public:
    virtual int sendLoginRequest(messageLogin* message){};
    virtual int sendUniQueryRequest(messageQueryRequest* message, char* courseInfo){};
    virtual int sendMultiQueryRequest(vector<messageQueryRequest>* message, vector<messageQueryMultiResponse>* response){};
};


class ServerMTCP{
private:
    string localIpAddr;
    int portNum;
    int listenQueue;
    ServerParentTcpSocket* serverMParentTcpSocket;
    ServerChildTcpSocket* serverMChildTcpSocket;
    UdpInterface* serverMUdp;

public:
    ServerMTCP(){
        localIpAddr = "127.0.0.1";
        portNum = 25410;
        listenQueue = 10;
        serverMParentTcpSocket = new ServerParentTcpSocket(localIpAddr, portNum, listenQueue);
        serverMChildTcpSocket = serverMParentTcpSocket->getServerChildTcpSocket();
    }

    void operator()(){
        while(true){
            int type = 0;
            if(recv(serverMChildTcpSocket->getSocketID(), &type, sizeof(type), 0) == -1){
                perror("ServerM failed to receive message from TCP socket.\n");
                continue;
            }
            if(type == CLIENT_LOGIN_IN_REQUEST) {
                messageLogin message;
                memset(&message, 0, sizeof(message));
                if (recv(serverMChildTcpSocket->getSocketID(), &message, sizeof(message), 0) == -1) {
                    perror("ServerM failed to receive message from TCP socket.\n");
                    continue;
                }
                printf("The main server received the authentication for %s using TCP over port %d.\n", message.username, portNum);
                encryptLogin(&message);
                int result = serverMUdp->sendLoginRequest(&message);
                if(result == -1){
                    printf("The main server failed to finish the login request.\n");
                    continue;
                }
                if(send(serverMChildTcpSocket->getSocketID(), (char *) &result, sizeof(result), 0) == -1)
                    perror("ServerM failed to send message to TCP socket.\n");
                else printf("The main server sent the authentication result to the client.\n");
            }

            if(type == CLIENT_QUERY_UNI_REQUEST){
                messageQueryRequest message;
                memset(&message, 0, sizeof(message));
                if(recv(serverMChildTcpSocket->getSocketID(), &message, sizeof(message), 0) == -1) {
                    perror("ServerM failed to receive message from TCP socket.\n");
                    continue;
                }
                printf("The main server received from %s to query course %s about %s using TCP over port %d.\n",
                       message.username, message.courseCode, message.category, portNum);

                char courseInfo[50];
                int result = serverMUdp->sendUniQueryRequest(&message, courseInfo);
                if(result == -1) {
                    printf("The main server failed to finish the query request.\n");
                    continue;
                }
                if(send(serverMChildTcpSocket->getSocketID(), (char*) &result, sizeof(result), 0) == -1) {
                    perror("ServerM failed to send message to TCP socket.\n");
                    continue;
                }
                if(result == SERVER_QUERY_RESPONSE_SUCCESS)
                    if(send(serverMChildTcpSocket->getSocketID(), courseInfo, sizeof(char[50]), 0) == -1) {
                        perror("ServerM failed to send message to TCP socket.\n");
                        continue;
                    }
                printf("The main server sent the query information to the client.\n");
            }

            if(type == CLIENT_QUERY_MULTI_REQUEST){
                //server receives the size of the request
                int size = 0;
                if(recv(serverMChildTcpSocket->getSocketID(), &size, sizeof(size), 0) == -1) {
                    perror("ServerM failed to receive message from TCP socket.\n");
                    continue;
                }
                //server receives the content of the request
                vector<messageQueryRequest> messageRequest;
                for(int i = 0; i < size; i++){
                    messageQueryRequest request;
                    if(recv(serverMChildTcpSocket->getSocketID(), &request, sizeof(request), 0) == -1) {
                        perror("ServerM failed to receive message from TCP socket.\n");
                        continue;
                    }
                    messageRequest.push_back(request);
                }
                printf("The main server received from %s to query course: ", messageRequest.front().username);
                printf(" to query course ");
                for(messageQueryRequest request: messageRequest)
                    printf("%s ", request.courseCode);
                printf("using TCP over port %d.\n", portNum);
                //server sends and receives the request to ServerCS/EE
                vector<messageQueryMultiResponse> messageResponse;

                if(serverMUdp->sendMultiQueryRequest(&messageRequest, &messageResponse) == -1) {
                    printf("The main server failed to finish the query request.\n");
                    continue;
                }
                for(messageQueryMultiResponse response: messageResponse){
                    if(send(serverMChildTcpSocket->getSocketID(), (char*) &response, sizeof(response), 0) == -1) {
                        perror("ServerM failed to send message to TCP socket.\n");
                        continue;
                    }
                }
                printf("The main server sent the query information to the client.\n");
            }
        }
    }

    void encryptLogin(messageLogin* message){
        for(int i = 0; i < 50 && message->username[i] != 0; i++){
            char c = message->username[i];
            if(c >= 'a' && c <= 'z')
                message->username[i] = (char)(((c - 'a') + 4) % 26 + 'a');
            if(c >= 'A' && c <= 'Z')
                message->username[i] = (char)(((c - 'A') + 4) % 26 + 'A');
            if(c >= '0' && c <= '9')
                message->username[i] = (char)(((c - '0') + 4) % 10 + '0');
        }
        for(int i = 0; i < 50 && message->password[i] != 0; i++){
            char c = message->password[i];
            if(c >= 'a' && c <= 'z')
                message->password[i] = (char)(((c - 'a') + 4) % 26 + 'a');
            if(c >= 'A' && c <= 'Z')
                message->password[i] = (char)(((c - 'A') + 4) % 26 + 'A');
            if(c >= '0' && c <= '9')
                message->password[i] = (char)(((c - '0') + 4) % 10 + '0');
        }
    }

    void setUDP(UdpInterface* serverMUdp){
        this->serverMUdp = serverMUdp;
    }

    ServerParentTcpSocket* getParentTcpSocket(){
        return this->serverMParentTcpSocket;
    }

};

class ServerMUDP: public UdpInterface{
private:
    string localIpAddr;
    int portNum;
    ClientUdpSocket* clientUdpSocket;
    sockaddr_in serverCAddr, serverEEAddr, serverCSAddr;

public:
    ServerMUDP(){
        localIpAddr = "127.0.0.1";
        portNum = 24410;
        clientUdpSocket = new ClientUdpSocket(localIpAddr, portNum);
        //initialize the address of serverC
        memset(&serverCAddr, 0, sizeof(serverCAddr));
        serverCAddr.sin_family = AF_INET;
        serverCAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverCAddr.sin_port = htons(21410);
        //initialize the address of serverCS
        memset(&serverCSAddr, 0, sizeof(serverCAddr));
        serverCSAddr.sin_family = AF_INET;
        serverCSAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverCSAddr.sin_port = htons(22410);
        //initialize the address of serverEE
        memset(&serverEEAddr, 0, sizeof(serverCAddr));
        serverEEAddr.sin_family = AF_INET;
        serverEEAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverEEAddr.sin_port = htons(23410);
    }

    int sendLoginRequest(messageLogin* message){
        int type = CLIENT_LOGIN_IN_REQUEST;
        if(sendto(clientUdpSocket->getSocketID(), (char*) &type, sizeof(type), 0, (struct sockaddr*)&serverCAddr, sizeof(serverCAddr)) == -1) {
            perror("ServerM failed to send message to UDP socket");
            return -1;
        }
        if(sendto(clientUdpSocket->getSocketID(), (char*) message, sizeof(*message), 0, (struct sockaddr*)&serverCAddr, sizeof(serverCAddr)) == -1) {
            perror("ServerM failed to send message to UDP socket");
            return -1;
        }
        printf("The main server sent an authentication request to serverC.\n");

        sockaddr_in serverAddr;
        socklen_t serverAddrLen = sizeof(serverAddr);
        int result = 0;
        if(recvfrom(clientUdpSocket->getSocketID(), &result, sizeof(result), 0, (struct sockaddr*) &serverAddr, &serverAddrLen) == -1){
            perror("ServerM failed to receive message from UDP socket.\n");
            return -1;
        }
        printf("The main server received the result of the authentication request from ServerC using UDP over port %d.\n", portNum);
        return result;
    }

    int sendUniQueryRequest(messageQueryRequest* message, char* courseInfo){
        if(message->department == CLIENT_QUERY_REQUEST_CS) {
            //send the type of the request to server
            int type = CLIENT_QUERY_UNI_REQUEST;
            if (sendto(clientUdpSocket->getSocketID(), (char *) &type, sizeof(type), 0,
                       (struct sockaddr *) &serverCSAddr, sizeof(serverCSAddr)) == -1) {
                perror("ServerM failed to send message to UDP socket");
                return -1;
            }
            //send the content of the request to serverCS
            if (sendto(clientUdpSocket->getSocketID(), (char *) message, sizeof(*message), 0,
                       (struct sockaddr *) &serverCSAddr, sizeof(serverCSAddr)) == -1) {
                perror("ServerM failed to send message to UDP socket");
                return -1;
            }
            printf("The main server sent a request to serverCS.\n");

            sockaddr_in serverAddr;
            socklen_t serverAddrLen = sizeof(serverAddr);
            int result = 0;
            if (recvfrom(clientUdpSocket->getSocketID(), &result, sizeof(result), 0, (struct sockaddr *) &serverAddr,
                         &serverAddrLen) == -1) {
                perror("ServerM failed to receive message from UDP socket.\n");
                return -1;
            }
            if (result == SERVER_QUERY_RESPONSE_SUCCESS) {
                if (recvfrom(clientUdpSocket->getSocketID(), courseInfo, sizeof(char[50]), 0,
                             (struct sockaddr *) &serverAddr, &serverAddrLen) == -1) {
                    perror("ServerM failed to receive message from UDP socket.\n");
                    return -1;
                }
            }
            printf("The main server received the response from serverCS using UDP over port %d.\n", portNum);
            return result;
        }

        if(message->department == CLIENT_QUERY_REQUEST_EE) {
            //send the type of the request to serverCS
            int type = CLIENT_QUERY_UNI_REQUEST;
            if (sendto(clientUdpSocket->getSocketID(), (char *) &type, sizeof(type), 0,
                       (struct sockaddr *) &serverEEAddr, sizeof(serverEEAddr)) == -1) {
                perror("ServerM failed to send message to UDP socket");
                return -1;
            }
            //send the content of the request to serverEE
            if (sendto(clientUdpSocket->getSocketID(), (char *) message, sizeof(*message), 0,
                       (struct sockaddr *) &serverEEAddr, sizeof(serverEEAddr)) == -1) {
                perror("ServerM failed to send message to UDP socket");
                return -1;
            }
            printf("The main server sent a request to serverEE.\n");

            sockaddr_in serverAddr;
            socklen_t serverAddrLen = sizeof(serverAddr);
            int result = 0;
            if (recvfrom(clientUdpSocket->getSocketID(), &result, sizeof(result), 0, (struct sockaddr *) &serverAddr,
                         &serverAddrLen) == -1) {
                perror("ServerM failed to receive message from UDP socket.\n");
                return -1;
            }
            if (result == SERVER_QUERY_RESPONSE_SUCCESS) {
                if (recvfrom(clientUdpSocket->getSocketID(), courseInfo, sizeof(char[50]), 0,
                             (struct sockaddr *) &serverAddr, &serverAddrLen) == -1) {
                    perror("ServerM failed to receive message from UDP socket.\n");
                    return -1;
                }
            }
            printf("The main server received the response from serverEE using UDP over port %d.\n", portNum);
            return result;
        }
    }

    int sendMultiQueryRequest(vector<messageQueryRequest>* messageRequest, vector<messageQueryMultiResponse>* messageResponse){
        vector<messageQueryRequest> requestCS, requestEE;

        for(messageQueryRequest request: *messageRequest){
            if(request.department == CLIENT_QUERY_REQUEST_CS){
                requestCS.push_back(request);
            }
            if(request.department == CLIENT_QUERY_REQUEST_EE){
                requestEE.push_back(request);
            }
        }
        if(requestCS.size() > 0){
            //send the type of the request to serverCS
            int type = CLIENT_QUERY_MULTI_REQUEST;
            if (sendto(clientUdpSocket->getSocketID(), (char *) &type, sizeof(type), 0,
                       (struct sockaddr *) &serverCSAddr, sizeof(serverCSAddr)) == -1) {
                perror("ServerM failed to send message to UDP socket");
                return -1;
            }
            //send the size of the request to serverCS
            int size = requestCS.size();
            if (sendto(clientUdpSocket->getSocketID(), (char *) &size, sizeof(size), 0,
                       (struct sockaddr *) &serverCSAddr, sizeof(serverCSAddr)) == -1) {
                perror("ServerM failed to send message to UDP socket");
                return -1;
            }
            //send the content of the request to serverCS
            for(messageQueryRequest message: requestCS){
                if (sendto(clientUdpSocket->getSocketID(), (char *) &message, sizeof(message), 0,
                           (struct sockaddr *) &serverCSAddr, sizeof(serverCSAddr)) == -1) {
                    perror("ServerM failed to send message to UDP socket");
                    return -1;
                }
            }
            printf("The main server sent a request to serverCS.\n");
            sockaddr_in serverAddr;
            socklen_t serverAddrLen = sizeof(serverAddr);
            for(int i = 0; i < requestCS.size(); i++){
                messageQueryMultiResponse response;
                if (recvfrom(clientUdpSocket->getSocketID(), &response, sizeof(response), 0, (struct sockaddr *) &serverAddr, &serverAddrLen) == -1) {
                    perror("ServerM failed to receive message from UDP socket.\n");
                    return -1;
                }
                messageResponse->push_back(response);
            }
            printf("The main server received the response from serverCS using UDP over port %d.\n", portNum);
        }

        if(requestEE.size() > 0){
            //send the type of the request to serverEE
            int type = CLIENT_QUERY_MULTI_REQUEST;
            if (sendto(clientUdpSocket->getSocketID(), (char *) &type, sizeof(type), 0,
                       (struct sockaddr *) &serverEEAddr, sizeof(serverEEAddr)) == -1) {
                perror("ServerM failed to send message to UDP socket");
                return -1;
            }
            //send the size of the request to serverEE
            int size = requestEE.size();
            if (sendto(clientUdpSocket->getSocketID(), (char *) &size, sizeof(size), 0,
                       (struct sockaddr *) &serverEEAddr, sizeof(serverEEAddr)) == -1) {
                perror("ServerM failed to send message to UDP socket");
                return -1;
            }
            //send the content of the request to serverEE
            for(messageQueryRequest message: requestEE){
                if (sendto(clientUdpSocket->getSocketID(), (char *) &message, sizeof(message), 0,
                           (struct sockaddr *) &serverEEAddr, sizeof(serverEEAddr)) == -1) {
                    perror("ServerM failed to send message to UDP socket");
                    return -1;
                }
            }
            printf("The main server sent a request to serverEE.\n");
            sockaddr_in serverAddr;
            socklen_t serverAddrLen = sizeof(serverAddr);
            for(int i = 0; i < size; i++){
                messageQueryMultiResponse response;
                if (recvfrom(clientUdpSocket->getSocketID(), &response, sizeof(response), 0, (struct sockaddr *) &serverAddr, &serverAddrLen) == -1) {
                    perror("ServerM failed to receive message from UDP socket.\n");
                    return -1;
                }
                messageResponse->push_back(response);
            }
            printf("The main server received the response from serverEE using UDP over port %d.\n", portNum);
        }
        return 1;
    }

};

int main(){
    cout << "The main server is up and running.\n";
    ServerMTCP serverMTcp;
    ServerMUDP serverMUdp;
    serverMTcp.setUDP(&serverMUdp);
    thread threadTCP(serverMTcp);
    thread threadParentTcpSocket(*serverMTcp.getParentTcpSocket());
    threadTCP.join();
    threadParentTcpSocket.join();
}

