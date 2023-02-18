//
// Created by Leshi Chen on 2022/11/28.
//
#include <iostream>
#include <cstring>
#include <fstream>
#include <thread>
#include <stdio.h>
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

class ServerUdpSocket{
private:
    string localIpAddr;
    int localPort;
    sockaddr_in serverAddr;
    int socketID;


public:
    ServerUdpSocket(string localIpAddr, int localPort){
        this->localIpAddr = localIpAddr;
        this->localPort = localPort;
        createServerUdpSocket();
        bindServerUdpSocket();
    }

    void createServerUdpSocket() {
        //create UDP socket
        socketID = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketID == -1) {
            perror("The UDP socket of server failed to create.\n");
            exit(0);
        }
    }

    void bindServerUdpSocket() {
        //initialize IP address, port number
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(localIpAddr.c_str());
        serverAddr.sin_port = htons(localPort);

        //bind UDP socket to the IP address and the port number
        if (::bind(socketID, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1) {
            perror("The UDP socket of server failed to bind. Please try again.\n");
            exit(0);
        }
    }

    int getSocketID() {
        return socketID;
    }
};

class ServerC{
private:
    string localIpAddr;
    int portNum;
    ServerUdpSocket* serverUdpSocket;

public:
    ServerC(){
        localIpAddr = "127.0.0.1";
        portNum = 21410;
        cout << "The ServerC is up and running using UDP on port " << portNum << "\n";
        serverUdpSocket = new ServerUdpSocket(localIpAddr, portNum);
    }

    void operator()(){
        while(true){
            int type = 0;
            sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            memset(&clientAddr, 0, sizeof(clientAddr));
            if(recvfrom(serverUdpSocket->getSocketID(), &type, sizeof(type), 0, (struct sockaddr*)&clientAddr, &clientAddrLen) == -1){
                perror("ServerC failed to receive message from UDP socket.\n");
                continue;
            }
            if(type == CLIENT_LOGIN_IN_REQUEST){
                messageLogin message;
                memset(&message, 0, sizeof(message));
                if(recvfrom(serverUdpSocket->getSocketID(), &message, sizeof(message), 0, (struct sockaddr*)&clientAddr, &clientAddrLen) == -1){
                    perror("ServerC failed to receive message from UDP socket.\n");
                    continue;
                }
                printf("The ServerC received an authentication request from the Main Server\n");
                int result = clientAuthentication(&message);
                if(sendto(serverUdpSocket->getSocketID(), (char*)&result, sizeof(result), 0, (struct sockaddr*)&clientAddr, clientAddrLen) == -1)
                    perror("ServerC failed to send message to UDP socket.\n");
                else printf("The ServerC finished sending the response to the Main Server.\n");
            }
        }
    }

    int clientAuthentication(messageLogin* message){
        ifstream credFile;
        credFile.open("cred.txt", ios::in);
        if(!credFile.is_open()){
            printf("file failed to open\n");
        }
        string line;
        while(getline(credFile, line)){
            string newLine = line.substr(0, line.find("\r"));
            int posSplit = newLine.find(",");
            string str1 = newLine.substr(0, posSplit);
            string str2 = newLine.substr(posSplit + 1, newLine.length() - posSplit - 1);
            char username[50];
            char password[50];
            strcpy(username, str1.c_str());
            strcpy(password, str2.c_str());
            if(strcmp(username, message->username) == 0)
                if(strcmp(password, message->password) == 0)
                    return SERVER_LOGIN_IN_RESPONSE_SUCCESS;
                else return SERVER_LOGIN_IN_RESPONSE_PASSWORD_ERROR;
        }
        credFile.close();
        return SERVER_LOGIN_IN_RESPONSE_USERNAME_ERROR;
    }
};

int main() {
    ServerC serverC;
    thread threadServerC(serverC);
    threadServerC.join();
}
