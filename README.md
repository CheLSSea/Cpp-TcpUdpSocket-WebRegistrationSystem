# Cpp-TcpUdpSocket-WebRegistrationSystem

a. Running Enviroment: 
Linux (Mac or Ubuntu 16.04) 
   
b. My code files and what they do:

1) serverM.cpp
It creates a TCP server socket to communicate with the TCP client socket in the client. It also creates a UDP client socket to dynamically communicate with each UDP server socket in serverC, serverCS and serverEE.

serverM plays a connected joint between the client and the backend servers. It transmits the login requests from the client, which are encrypted by serverM and sent to the serverC, and sends back the authenticated result from the serverC to the client. Besides, serverM also transmits the query requests about courses from the client, which are distributed to the serverCS or serverEE, and sends back the query result from the serverCS or serverEE to the client.
   
2) serverC.cpp
It creates a UDP server socket connecting to the serverM.

serverC is responsible for authentication of login request from the client. It compares the username and password sent by the client to the correct username and password which are saved in a txt file.

3) serverCS.cpp
It creates a UDP server socket connecting to the serverM.

serverCS focuses on looking for the information about the CS courses and returning the query results to the client. The information of courses are saved as a txt file.
   
4) serverEE.cpp
It creates a UDP server socket connecting to the serverM.
  
serverEE focuses on looking for the information about the EE courses and returning the query results to the client. The information of courses are saved as a txt file.
   
5) client.cpp
It creates a TCP client socket connecting to the serverM.

client can input username and password to login and enter the course code and category to querying for the expected information.

c. The format of all the messages exchanged:
1) Login request from client to serverC:
-- int type = CLIENT_LOGIN_IN_REQUEST;
-- struct messageLogin{
   char username[50];
   char password[50];
}

2) Login response from serverC to client: 
-- int result = SERVER_LOGIN_IN_RESPONSE_SUCCESS / SERVER_LOGIN_IN_RESPONSE_USERNAME_ERROR / SERVER_LOGIN_IN_RESPONSE_PASSWORD_ERROR;

3) Query request from client to serverCS/serverEE:
-- int type = CLIENT_QUERY_UNI_REQUEST / CLIENT_QUERY_MULTI_REQUEST;
-- struct messageQueryRequest{
   char username[50];
   int department = CLIENT_QUERY_REQUEST_CS / CLIENT_QUERY_REQUEST_EE;
   char courseCode[20];
   char category[20];
}

4)Query response from serverCS/serverEE to client:
For single request:
-- int result = SERVER_QUERY_RESPONSE_SUCCESS / SERVER_QUERY_RESPONSE_FAIL;
-- char courseInfo[50];

For multiple requests:
-- struct messageQueryMultiResponse{
   int result = SERVER_QUERY_RESPONSE_SUCCESS / SERVER_QUERY_RESPONSE_FAIL;
   char courseCode[10];
   char content[120];
}

d. Any idiosyncrasy of my project: 
We can know from my messages exchanged that if the length of the messages exceeds the maximum capacity of the variables, then the sockets could not transmit complete messages.

e. Reused Code: I didn't use code from anywhere.
 
