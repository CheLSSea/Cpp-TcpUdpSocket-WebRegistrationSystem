//
// Created by Leshi Chen on 2022/11/28.
//

#include <cstring>
#include <cstdio>
#include <iostream>
#include <vector>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

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

class ClientTcpSocket{
private:
    int socketID;
    string localIpAddr;
    int serverPort;
    struct sockaddr_in serverAddr;

public:
    ClientTcpSocket(string ipAddr, int serverPort){
        this->localIpAddr = localIpAddr;
        this->serverPort = serverPort;
        createSocket();
        connectServer();
    }

    void createSocket(){
        socketID = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socketID == -1) {
            perror("The TCP socket of client failed to create. Please try again.\n");
            exit(0);
        }
    }
    void connectServer(){
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverAddr.sin_port = htons(25410);
        if (::connect(socketID, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1) {
            perror("The TCP socket of client failed to connect server. Please try again.\n");
            exit(0);
        }
    }

    int getSocketID(){
        return socketID;
    }

    int getSocketPort(){
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        getsockname(socketID, (struct sockaddr*) &clientAddr, &clientAddrLen);
        return clientAddr.sin_port;
    }
};


class Client{
private:
    ClientTcpSocket *socket; //Client TCP Socket
    char username[50]; //record the username of the client
    char password[50]; //record the password of the client

public:
    Client(){
        cout << "The client is up and running\n";
        socket = new ClientTcpSocket("127.0.0.1", 25410);
        userLogin();
    }

    void userLogin() {
        int attempt = 3;
        while (attempt > 0) {
            cout << "Please enter the username:";
            cin >> username;
            cout << "Please enter the password:";
            cin >> password;
            if (authentication()) {
                queryRequest();
                return;
            }
            attempt--;
            if(attempt > 0)
                cout << "Attempts remaining: " << attempt << "\n";
            else{
                cout << "Authentication Failed for 3 attempts. Client will shut down.\n";
                exit(0);
            }
        }
    }

    bool authentication() {
        cout << username << " send an authentication request to the main server.\n";
        //client send type of the message to server for authentication
        int type = CLIENT_LOGIN_IN_REQUEST;
        send(socket->getSocketID(), (char *) &type, sizeof(type), 0);
        //client send username and password to server for authentication
        messageLogin message;
        memset(&message, 0, sizeof(message));
        //message.type = CLIENT_LOGIN_IN_REQUEST;
        strcpy(message.username, username);
        strcpy(message.password, password);
        send(socket->getSocketID(), (char *) &message, sizeof(message), 0);

        //client waits for the result of authentication from server
        int result = 0;
        recv(socket->getSocketID(), &result, sizeof(result), 0);
        switch (result) {
            case SERVER_LOGIN_IN_RESPONSE_SUCCESS:
                cout << username << " received the result of authentication using TCP over port "
                     << socket->getSocketPort() << ". Authentication is successful.\n";
                return true;
            case SERVER_LOGIN_IN_RESPONSE_USERNAME_ERROR:
                cout << username << " received the result of authentication using TCP over port "
                     << socket->getSocketPort() << ". Authentication failed: Username Does not exist.\n";
                return false;
            case SERVER_LOGIN_IN_RESPONSE_PASSWORD_ERROR:
                cout << username << " received the result of authentication using TCP over port "
                     << socket->getSocketPort() << ". Authentication failed: Password does not match.\n";
                return false;
            default:
                cout << "Message is an error.\n";
                return false;
        }
    }

    void queryRequest(){
        while(true){
            //client sends the type of request to serverM
            cout << "Please enter the course code to query:";
            vector<string> courses;
            int n = 0;
            while(n++ < 10){
                string str;
                cin >> str;
                courses.push_back(str);
                char c = getchar();
                if(c == '\n')
                    break;
            }
            if(courses.size() < 1)
                continue;
            else if(courses.size() == 1)
                uniQueryRequest(&courses);
            else multiQueryRequest(&courses);
            cout << "\n-----Start a new request-----\n";
        }

    }

    void uniQueryRequest(vector<string>* courses){
        //client inputs the category
        string category;
        cout << "Please enter the category (Credit / Professor / Days / CourseName):";
        cin >> category;

        //client sends the type of the request to server
        int type = CLIENT_QUERY_UNI_REQUEST;
        if(send(socket->getSocketID(), (char *) &type, sizeof(type), 0) == -1){
            perror("The client failed to send a message to TCP socket.\n");
            return;
        }

        //client sets the content of the request message
        messageQueryRequest messageRequest;
        string courseCode = courses->front();
        string dept = courseCode.substr(0, 2);
        if(dept.compare("CS") == 0)
            messageRequest.department = CLIENT_QUERY_REQUEST_CS;
        else if(dept.compare("EE") == 0)
            messageRequest.department = CLIENT_QUERY_REQUEST_EE;
        else{
            cout << courseCode << "is invalid course code. Please enter the course code of CS or EE.\n";
            return;
        }
        strcpy(messageRequest.username, username);
        strcpy(messageRequest.courseCode, courseCode.c_str());
        strcpy(messageRequest.category, category.c_str());

        //client sends the content of request to serverM
        if(send(socket->getSocketID(), (char *) &messageRequest, sizeof(messageRequest), 0) == -1){
            perror("The client failed to send a message to TCP socket.\n");
            return;
        }
        cout << username << " sent a request to the main server.\n";

        //client receives the result of query from serverM
        int result = 0;
        if(recv(socket->getSocketID(), &result, sizeof(result), 0) == -1) {
            perror("The client failed to receive a message from TCP socket.\n");
            return;
        }
        if(result == SERVER_QUERY_RESPONSE_FAIL){
            cout << "Didn't find the course: " << courseCode << ".\n";
            return;
        }
        char courseInfo[50];
        if(recv(socket->getSocketID(), courseInfo, sizeof(char[50]), 0) == -1){
            perror("The client failed to receive a message from TCP socket.\n");
            return;
        }
        cout << "The client received the response from the Main server using TCP over port " << socket->getSocketPort() <<".\n";
        printf("The %s of %s is %s.\n", category.c_str(), courseCode.c_str(), courseInfo);
    }

    void multiQueryRequest(vector<string>* courses){
        //client sends the type of the request to server
        int type = CLIENT_QUERY_MULTI_REQUEST;
        if(send(socket->getSocketID(), (char *) &type, sizeof(type), 0) == -1){
            perror("The client failed to send a message to TCP socket.\n");
            return;
        }

        //client sets the content of the request message
        vector<messageQueryRequest> messageRequest;
        for(string courseCode: *courses){
            messageQueryRequest request;
            string dept = courseCode.substr(0, 2);
            if(dept.compare("CS") == 0)
                request.department = CLIENT_QUERY_REQUEST_CS;
            else if(dept.compare("EE") == 0)
                request.department = CLIENT_QUERY_REQUEST_EE;
            else{
                cout << courseCode << "is invalid course code. This one will be abandoned.\n";
                continue;
            }
            strcpy(request.username, username);
            strcpy(request.courseCode, courseCode.c_str());
            messageRequest.push_back(request);
        }

        //client sends the size of request to serverM
        int size = messageRequest.size();
        if(send(socket->getSocketID(), (char *) &size, sizeof(size), 0) == -1){
            perror("The client failed to send a message to TCP socket.\n");
            return;
        }
        //client sends the content of request to serverM
        for(messageQueryRequest request: messageRequest){
            if(send(socket->getSocketID(), (char *) &request, sizeof(request), 0) == -1){
                perror("The client failed to send a message to TCP socket.\n");
                return;
            }
        }
        cout << username << " sent a request with multiple CourseCode to the main server.\n";

        //client receives the result of query from serverM
        vector<messageQueryMultiResponse> messageResponse;
        for(int i = 0; i < size; i++){
            messageQueryMultiResponse response;
            if(recv(socket->getSocketID(), &response, sizeof(response), 0) == -1){
                perror("The client failed to receive a message from TCP socket.\n");
                return;
            }
            messageResponse.push_back(response);
        }
        cout << "The client received the response from the Main server using TCP over port " << socket->getSocketPort() <<".\n";
        cout << "CourseCode: Credits,Professor,Days,CourseName\n";
        for(messageQueryMultiResponse response: messageResponse){
            if(response.result == SERVER_QUERY_RESPONSE_SUCCESS)
                printf("%s: %s\n", response.courseCode, response.content);
            else cout << "Didn't find the course: " << response.courseCode;
        }
    }

};

int main(){
    Client client;
}
