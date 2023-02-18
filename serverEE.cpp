//
// Created by Leshi Chen on 2022/11/28.
//
#include <iostream>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>
#include <map>
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

class ServerEE{
private:
    string localIpAddr;
    int portNum;
    ServerUdpSocket* serverUdpSocket;

public:
    ServerEE(){
        localIpAddr = "127.0.0.1";
        portNum = 23410;
        cout << "The ServerEE is up and running using UDP on port " << portNum << "\n";
        serverUdpSocket = new ServerUdpSocket(localIpAddr, portNum);
    }

    void operator()(){
        while(true){
            sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            memset(&clientAddr, 0, sizeof(clientAddr));
            int type = 0;
            if(recvfrom(serverUdpSocket->getSocketID(), &type, sizeof(type), 0, (struct sockaddr*)&clientAddr, &clientAddrLen) == -1){
                perror("ServerCS failed to receive message from UDP socket.\n");
                continue;
            }
            if(type == CLIENT_QUERY_UNI_REQUEST) {
                messageQueryRequest messageRequest;
                memset(&messageRequest, 0, sizeof(messageRequest));
                if (recvfrom(serverUdpSocket->getSocketID(), &messageRequest, sizeof(messageRequest), 0,
                             (struct sockaddr *) &clientAddr, &clientAddrLen) == -1) {
                    perror("ServerEE failed to receive message from UDP socket.\n");
                    continue;
                }
                printf("The ServerEE received a request from the Main Server about the %s of %s.\n",
                       messageRequest.category, messageRequest.courseCode);
                char courseInfo[50];
                if (queryUniCourse(&messageRequest, courseInfo) == 1) {
                    int result = SERVER_QUERY_RESPONSE_SUCCESS;
                    if (sendto(serverUdpSocket->getSocketID(), (char *) &result, sizeof(result), 0,
                               (struct sockaddr *) &clientAddr, clientAddrLen) == -1) {
                        perror("ServerEE failed to send message to UDP socket.\n");
                        continue;
                    }
                    if (sendto(serverUdpSocket->getSocketID(), courseInfo, sizeof(char[50]), 0,
                               (struct sockaddr *) &clientAddr, clientAddrLen) == -1) {
                        perror("ServerEE failed to send message to UDP socket.\n");
                        continue;
                    }
                } else {
                    int result = SERVER_QUERY_RESPONSE_FAIL;
                    if (sendto(serverUdpSocket->getSocketID(), (char *) &result, sizeof(result), 0,
                               (struct sockaddr *) &clientAddr, clientAddrLen) == -1) {
                        perror("ServerEE failed to send message to UDP socket.\n");
                        continue;
                    }
                }
                printf("The ServerEE finished sending the response to the Main Server.\n");
            }

            if(type == CLIENT_QUERY_MULTI_REQUEST){
                int size = 0;
                if(recvfrom(serverUdpSocket->getSocketID(), &size, sizeof(size), 0, (struct sockaddr*)&clientAddr, &clientAddrLen) == -1){
                    perror("ServerEE failed to receive message from UDP socket.\n");
                    continue;
                }
                vector<messageQueryRequest> messageRequest;
                for(int i = 0; i < size; i++){
                    messageQueryRequest request;
                    if(recvfrom(serverUdpSocket->getSocketID(), &request, sizeof(request), 0, (struct sockaddr*)&clientAddr, &clientAddrLen) == -1){
                        perror("ServerEE failed to receive message from UDP socket.\n");
                        continue;
                    }
                    messageRequest.push_back(request);
                }
                vector<messageQueryMultiResponse> messageResponse = queryMultiCourse(&messageRequest);
                for(messageQueryMultiResponse response: messageResponse){
                    if (sendto(serverUdpSocket->getSocketID(), (char *) &response, sizeof(response), 0,
                               (struct sockaddr *) &clientAddr, clientAddrLen) == -1) {
                        perror("ServerEE failed to send message to UDP socket.\n");
                        continue;
                    }
                }
                printf("The ServerEE finished sending the response to the Main Server.\n");
            }
        }
    }

    int queryUniCourse(messageQueryRequest* message, char* info){
        ifstream csFile;
        csFile.open("ee.txt", ios::in);
        string line;
        while(getline(csFile, line)){
            string newline = line.substr(0, line.find("\r"));
            string courseInfo[5];
            for(int i = 0; i < 5; i++){
                int posSplit = newline.find(",");
                courseInfo[i] = newline.substr(0, posSplit);
                newline = newline.substr(posSplit + 1, newline.length() - posSplit - 1);
            }
            if(strcmp(message->courseCode, courseInfo[0].c_str()) != 0)
                continue;
            if(strcmp(message->category, "Credit") == 0){
                strcpy(info, courseInfo[1].c_str());
                printf("The course information has been found: The %s of %s is %s.\n",
                       message->category, message->courseCode, info);
                return 1;
            }
            if(strcmp(message->category, "Professor") == 0){
                strcpy(info, courseInfo[2].c_str());
                printf("The course information has been found: The %s of %s is %s.\n",
                       message->category, message->courseCode, info);
                return 1;
            }
            if(strcmp(message->category, "Days") == 0){
                strcpy(info, courseInfo[3].c_str());
                printf("The course information has been found: The %s of %s is %s.\n",
                       message->category, message->courseCode, info);
                return 1;
            }
            if(strcmp(message->category, "CourseName") == 0){
                strcpy(info, courseInfo[4].c_str());
                printf("The course information has been found: The %s of %s is %s.\n",
                       message->category, message->courseCode, info);
                return 1;
            }
        }
        printf("Didn't find the course: %s.\n", message->courseCode);
        csFile.close();
        return -1;
    }

    vector<messageQueryMultiResponse> queryMultiCourse(vector<messageQueryRequest>* messageRequest) {
        ifstream csFile;
        csFile.open("ee.txt", ios::in);
        string line;
        map<string, string> hashmap;
        while (getline(csFile, line)) {
            string newline = line.substr(0, line.find("\r"));
            int posSplit = newline.find(",");
            string course = newline.substr(0, posSplit);
            string content = newline.substr(posSplit + 1, newline.length() - posSplit - 1);
            hashmap.insert({course, content});
        }
        vector<messageQueryMultiResponse> messageResponse;
        for(messageQueryRequest request: *messageRequest){
            messageQueryMultiResponse response;
            response.result = 0;
            string course(request.courseCode, request.courseCode + strlen(request.courseCode));
            if(hashmap.count(course) > 0){
                response.result = SERVER_QUERY_RESPONSE_SUCCESS;
                strcpy(response.courseCode, request.courseCode);
                strcpy(response.content, hashmap[course].c_str());
                printf("The course information has been found: %s, %s\n", response.courseCode, response.content);
            }
            else{
                response.result = SERVER_QUERY_RESPONSE_FAIL;
                strcpy(response.courseCode, request.courseCode);
                printf("Didn't find the course: %s.\n", request.courseCode);
            }
            messageResponse.push_back(response);
        }
        csFile.close();
        return messageResponse;
    }
};


int main() {
    ServerEE serverEE;
    thread threadServerEE(serverEE);
    threadServerEE.join();
}
