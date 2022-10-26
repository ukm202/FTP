#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024 // BUFFER_SIZE

#define S_CONTROLPORT 21 // Control Channel
#define S_DATAPORT 20    // Data Channel

int createSocket(bool lstn, int sPort, int cPort);

// Array to maintain session state
int session[100];

// Struct that defines individual clients directory & login
typedef struct
{
    char *dir;
    int currentDataPort;
} Session;

// Array of Client Session
Session sess[100];

// Global Variables
char check_username[256];
int arr_size, cnt = 0;

// Object to load login details
typedef struct
{
    char *user;
    char *pass;
} login_details;

// Array that stores the login data
login_details data[100];

// load function which reads the user file from the directory and loads data into the array
int load(char *filename)
{
    char buffer[200];
    char token[50];
    login_details *log;
    FILE *file = fopen(filename, "r");
    int size = 0;
    char delim[] = ",";
    int x = 2;
    int k = 0;
    int i;
    char format_str[50] = "\0";
    while (fgets(buffer, 200, file) != NULL)
    {
        log = (login_details *)malloc(sizeof(login_details));
        char *ptr = strtok(buffer, delim);
        while (ptr != NULL)
        {
            if (x % 2 == 0)
            {
                log->user = strdup(ptr);
            }
            else if (x % 2 == 1)
            {
                for (i = 0; i < strlen(ptr); i++)
                {
                    k = ptr[i];
                    if (k > 31 && k < 127)
                    {
                        char c = k;
                        strcat(format_str, &c);
                    }
                    format_str[i + 1] = '\0';
                }
                format_str[i] = '\0';
                log->pass = strdup(format_str);
                bzero(format_str, sizeof(format_str));
            }
            ptr = strtok(NULL, delim);
            x++;
        }
        data[size] = *log;
        size++;
    }
    fclose(file);
    return size;
}
//-------------------------------------------------------------------------------------------------------

// function that returns appropriate response messages
char *responseMsg(int statusCode)
{
    char *response;
    switch (statusCode)
    {
    case 530:
    {
        char msg[] = "530 Not logged in";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 331:
    {
        char msg[] = "331 Username OK, need password ...";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 230:
    {
        char msg[] = "230 User logged in, please proceed ...";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 503:
    {
        char msg[] = "503 Bad sequence of commands";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 202:
    {
        char msg[] = "202 Command not implemented";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 550:
    {
        char msg[] = "550 No such file or directory";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 200:
    {
        char msg[] = "200 PORT Command Successful";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 150:
    {
        char msg[] = "150 File status okay; about to open data connection";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 226:
    {
        char msg[] = "226 Transfer completed";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }
    }

    return response;
}

// Bool Function that checks if User is logged in
bool userAuth(int client, char *command)
{
    if ((session[client] == -1 || session[client] == 0))
    {
        session[client] = -1;
        return false;
    }
    return true;
}

int parsePort(char *msg)
{
    int count = 0;
    for (; *msg != '\0'; msg++)
    {
        if (*msg == ',')
        {
            count++;
            if (count == 4)
            {
                msg++;
                break;
            }
        }
    }

    char *token = strtok(msg, ",");
    int p1 = atoi(token);
    token = strtok(NULL, ",");
    int p2 = atoi(token);
    return p1 * 256 + p2;
}

void performPORT(int client, char *buffer)
{
    char *msg = buffer + 5;
    int port = parsePort(msg);
    sess[client].currentDataPort = port;
    send(client, responseMsg(200), BUFFER_SIZE, 0);
}

// Function that returns server directory
void performPWD(int client, bool cwd_flag)
{
    char *cwd;
    chdir(sess[client].dir);
    if ((cwd = getcwd(NULL, 0)))
    {
        char msg[BUFFER_SIZE];
        if (!cwd_flag)
        {
            strcpy(msg, "257 ");
        }
        else
        {
            strcpy(msg, "200 Directory Changed to ");
        }
        strcat(msg, cwd);
        send(client, msg, BUFFER_SIZE, 0);
    }
    else
    {
        send(client, responseMsg(550), BUFFER_SIZE, 0);
    }
}

// Function to Change Server Directory
void performCWD(int client, char *buffer)
{
    chdir(sess[client].dir);
    char *directory = buffer + 4;
    if (chdir(directory) != 0)
    {
        send(client, responseMsg(550), BUFFER_SIZE, 0);
    }
    else
    {
        char u_dir[BUFFER_SIZE];
        getcwd(u_dir, BUFFER_SIZE);

        Session *s;
        s = (Session *)malloc(sizeof(Session));
        s->dir = strdup(u_dir);
        sess[client] = *s;

        performPWD(client, true);
    }
}

// Function to List files in current server directory
void performLIST(int client)
{
    send(client, responseMsg(150), BUFFER_SIZE, 0);
    int channel = createSocket(false, S_DATAPORT, sess[client].currentDataPort);

    int count = 0;
    struct dirent *dir;

    chdir(sess[client].dir);

    DIR *d;
    d = opendir(".");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (count > 1)
            {
                char line[BUFFER_SIZE];
                char type[] = {dir->d_type == 4 ? 'D' : 'F', '\0'};
                strcpy(line, type);
                strcat(line, "\t");
                strcat(line, dir->d_name);
                send(channel, line, BUFFER_SIZE, 0);
            }
            count++;
        }
        closedir(d);
    }

    close(channel);
}

// Function to Retrieve File from server directory
void performRETR(int client, char *filename)
{
    send(client, responseMsg(150), BUFFER_SIZE, 0);

    int channel = createSocket(false, S_DATAPORT, sess[client].currentDataPort);

    FILE *f;
    f = fopen(filename, "rb");

    if (f == NULL)
    {
        printf("Can't open %s\n", filename);
    }
    else
    {
        struct stat stat_buf;
        int temp;
        int rc = stat(filename, &stat_buf);
        int fsize = stat_buf.st_size;

        char databuff[fsize + 1];
        fread(databuff, 1, sizeof(databuff), f);
        int total = 0, bytesleft = fsize, ln;

        while (total < fsize)
        {
            ln = send(channel, databuff + total, bytesleft, 0);
            if (ln == -1)
            {
                break;
            }
            total += ln;
            bytesleft -= ln;
        }
        bzero(databuff, sizeof(databuff));
        fclose(f);
    }
    close(channel);
    send(client, responseMsg(226), BUFFER_SIZE, 0);
}

// Function to store File into server directory
void performSTOR(int client, char *filename)
{
    send(client, responseMsg(150), BUFFER_SIZE, 0);
    usleep(1000000);
    int channel = createSocket(false, S_DATAPORT, sess[client].currentDataPort);

    char reader[BUFFER_SIZE];
    bzero(reader, BUFFER_SIZE);

    FILE *f;
    f = fopen(filename, "wb");

    int total = 0, ln;

    while ((ln = read(channel, reader, BUFFER_SIZE)) > 0)
    {
        fwrite(reader, 1, BUFFER_SIZE, f);
        total += ln;
        if (ln < BUFFER_SIZE)
        {
            break;
        }
    }
    fclose(f);
    close(channel);
    send(client, responseMsg(226), BUFFER_SIZE, 0);
}

// Function to Check Username
void performUSER(int client, char *buffer)
{
    if (session[client] == 1 || session[client] == 0)
    {
        send(client, responseMsg(503), BUFFER_SIZE, 0);
        return;
    }

    char *username = buffer + 5;
    for (int i = 0; i < arr_size - 1; i++)
    {
        if (strcmp(username, data[i].user) == 0)
        {
            session[client] = 0;
            send(client, responseMsg(331), BUFFER_SIZE, 0);
            bzero(check_username, sizeof(check_username));
            memcpy(check_username, username, strlen(username));
            break;
        }
    }

    if (session[client] == -1)
    {
        send(client, responseMsg(530), BUFFER_SIZE, 0);
    }
}

// Function to Check password
void performPASS(int client, char *buffer)
{
    if (session[client] == 1 || session[client] == -1)
    {
        send(client, responseMsg(503), BUFFER_SIZE, 0);
        return;
    }

    char *password = buffer + 5;
    if (session[client] == 0)
    {
        for (int i = 0; i < arr_size - 1; i++)
        {
            if (strcmp(password, data[i].pass) == 0 && strcmp(check_username, data[i].user) == 0)
            {
                session[client] = 1;
                send(client, responseMsg(230), BUFFER_SIZE, 0);
                break;
            }
        }
        if (session[client] == 0)
        {
            session[client] = -1;
            send(client, responseMsg(530), BUFFER_SIZE, 0);
        }
    }
}

// Function that creates socket & connections
int createSocket(bool lstn, int sPort, int cPort)
{
    // create a socket - Control Channel Socket
    int cSocket;
    cSocket = socket(AF_INET, SOCK_STREAM, 0);

    // check for fail error - for control
    if (cSocket == -1)
    {
        printf("Socket creation failed.\n");
        exit(EXIT_FAILURE);
    }

    // setsock
    int value = 1; // scope is closed only until next line
    setsockopt(cSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

    struct sockaddr_in sAddr, cAddr;
    bzero(&sAddr, sizeof(sAddr));
    bzero(&cAddr, sizeof(cAddr));

    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(sPort);
    sAddr.sin_addr.s_addr = INADDR_ANY;

    if (!lstn)
    {
        cAddr.sin_family = AF_INET;
        cAddr.sin_port = htons(cPort);
        cAddr.sin_addr.s_addr = INADDR_ANY;

        // connecting to server port
        int connection_status =
            connect(cSocket,
                    (struct sockaddr *)&cAddr,
                    sizeof(cAddr));

        // check for errors with the connection
        if (connection_status == -1)
        {
            printf("There was an error making a connection to the remote socket. %d %d\n\n", cPort, sPort);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // bind the socket to our specified IP and port
        if (bind(cSocket, (struct sockaddr *)&sAddr, sizeof(sAddr)) < 0)
        {
            printf("Socket bind failed. %d %d\n", cPort, sPort);
            exit(EXIT_FAILURE);
        }
        // after it is bound, we can listen for connections with queue length of 5
        if (listen(cSocket, 5) < 0)
        {
            printf("Listen failed.\n");
            close(cSocket);
            exit(EXIT_FAILURE);
        }

        printf("LISTENING on PORT %d...\n", sPort);
    }

    return cSocket;
}

// Function that handles client input
void handleCommands(int client, char *buffer)
{
    char command[5];
    strncpy(command, buffer + 0, 4);
    printf("Command: %s\n", buffer);

    if (strcmp(command, "USER") == 0)
    {
        performUSER(client, buffer);
        return;
    }
    else if (strcmp(command, "PASS") == 0)
    {
        performPASS(client, buffer);
        return;
    }

    if (userAuth(client, command))
    {
        if (strcmp(command, "PWD") == 0)
        {
            performPWD(client, false);
        }
        else if (strstr(command, "CWD"))
        {
            performCWD(client, buffer);
        }
        else if (strstr(command, "PORT"))
        {
            performPORT(client, buffer);
        }
        else if (strcmp(command, "LIST") == 0)
        {
            int pid = fork();
            if (pid == 0)
            {
                performLIST(client);
                exit(EXIT_SUCCESS);
            }
        }
        else if (strcmp(command, "RETR") == 0)
        {
            int pid = fork();
            if (pid == 0)
            {
                performRETR(client, buffer + 5);
                exit(EXIT_SUCCESS);
            }
        }
        else if (strcmp(command, "STOR") == 0)
        {
            int pid = fork();
            if (pid == 0)
            {
                performSTOR(client, buffer + 5);
                exit(EXIT_SUCCESS);
            }
        }
        else
        {
            send(client, responseMsg(202), BUFFER_SIZE, 0);
        }
    }
    else if (strstr(command, "STOR") || strstr(command, "RETR") || strstr(command, "LIST") || strstr(command, "CWD") || strstr(command, "PWD"))
    {
        send(client, responseMsg(530), BUFFER_SIZE, 0);
    }
    else
    {
        send(client, responseMsg(202), BUFFER_SIZE, 0);
    }
}

// Main Function
int main()
{

    char *filename = "users.txt";
    arr_size = load(filename);

    char u_dir[256];
    getcwd(u_dir, 256);
    Session *s;
    for (int i = 0; i < 100; ++i)
    {
        session[i] = -1;
        s = (Session *)malloc(sizeof(Session));
        s->dir = strdup(u_dir);
        sess[i] = *s;
    }

    int sSocket = createSocket(true, S_CONTROLPORT, 0);

    // DECLARE 2 fd sets (file descriptor sets : a collection of file descriptors)
    fd_set all_sockets;
    fd_set ready_sockets;

    // zero out/iniitalize our set of all sockets
    FD_ZERO(&all_sockets);

    // adds one socket (the current socket) to the fd set of all sockets
    FD_SET(sSocket, &all_sockets);

    while (1)
    {
        ready_sockets = all_sockets;
        if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
        {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        for (int fd = 0; fd < FD_SETSIZE; fd++)
        {
            // check to see if that fd is SET
            if (FD_ISSET(fd, &ready_sockets))
            {
                if (fd == sSocket)
                {
                    // accept that new connection
                    int client_sd = accept(sSocket, 0, 0);

                    // add the newly accepted socket to the set of all sockets that we are watching
                    FD_SET(client_sd, &all_sockets);

                    session[fd] = -1;
                }
                else
                {
                    char buffer[BUFFER_SIZE];
                    bzero(buffer, sizeof(buffer));

                    int bytes = recv(fd, buffer, sizeof(buffer), 0);
                    if (bytes == 0) // client has closed the connection
                    {
                        printf("connection closed from client side \n");
                        // we are done, close fd
                        session[fd] = -1;
                        close(fd);
                        // once we are done handling the connection, remove the socket from the list of file descriptors that we are watching
                        FD_CLR(fd, &all_sockets);
                    }

                    // when data is received
                    handleCommands(fd, buffer);
                }
            }
        }
    }

    // close
    close(sSocket);
    return 0;
}