// use read for recv, write for send
// names - start from small letters + _ + small letters again



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

// function to create control channel - let's rename this variables and make this fuction together
int control_channel(int sPort, int cPort)
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
    bzero(&cAddr, sizeof(cAddr));
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

// Function that creates data channel
int data_channel(int sPort, int cPort)
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

    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(sPort);
    sAddr.sin_addr.s_addr = INADDR_ANY;


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
    

    return cSocket;
}



// load function which reads the user file from the directory and loads data into the array
int read_users_file(char *users_filename)
{
    char buffer[200];
    char token[50];
    login_details *log;
    FILE *f = fopen(users_filename, "r");
    int arr_size = 0;
    char delimitter[] = ",";
    int i, j, k;
    j = 2;
    k = 0;
    char string_format[50] = "\0";
    while (fgets(buffer, 200, f) != NULL)
    {
        log = (login_details *)malloc(sizeof(login_details));
        char *ptr = strtok(buffer, delimitter);
        while (ptr != NULL)
        {
            if (j % 2 == 0)
            {
                log->user = strdup(ptr);
            }
            else if (j % 2 == 1)
            {
                for (i = 0; i < strlen(ptr); i++)
                {
                    k = ptr[i];
                    if (k > 31 && k < 127)
                    {
                        char c = k;
                        strcat(string_format, &c);
                    }
                    string_format[i + 1] = '\0';
                }
                string_format[i] = '\0';
                log->pass = strdup(string_format);
                bzero(string_format, sizeof(string_format));
            }
            ptr = strtok(NULL, delimitter);
            j++;
        }
        data[arr_size] = *log;
        arr_size++;
    }
    fclose(f);
    return arr_size;
}
//-------------------------------------------------------------------------------------------------------

// function that returns appropriate response messages
char *ret_response(int status_code)
{
    char *response;
    // char STATUS_CODES[8][4] = {530, 331, 230, 503, 202, 550, 200, 150, 226}; // creating doesn't look suitable

    switch (status_code)
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
// do we need this second parameter command?
bool autheticate_user(int client, char *command)
{
    if ((session[client] == -1 || session[client] == 0))
    {
        session[client] = -1;
        return false;
    }
    return true;
}

int parse_port(char *text)
{
    int count = 0;
    for (; *text != '\0'; text++)
    {
        if (*text == ',')
        {
            count++;
            if (count == 4)
            {
                text++;
                break;
            }
        }
    }

    char *token = strtok(text, ",");
    int p1 = atoi(token);
    token = strtok(NULL, ",");
    int p2 = atoi(token);
    return p1 * 256 + p2;
}

void port_cmd(int client, char *buffer)
{
    char *msg = buffer + 5;
    int port = parse_port(msg);
    sess[client].currentDataPort = port;
    send(client, ret_response(200), BUFFER_SIZE, 0);
}

// Function that returns server directory
void pwd_cmd(int client, bool cwd_flag)
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
        send(client, ret_response(550), BUFFER_SIZE, 0);
    }
}

// Function to Change Server Directory
void cwd_cmd(int client, char *buffer)
{
    chdir(sess[client].dir);
    char *directory = buffer + 4;
    if (chdir(directory) != 0)
    {
        send(client, ret_response(550), BUFFER_SIZE, 0);
    }
    else
    {
        char user_dir[BUFFER_SIZE];
        getcwd(user_dir, BUFFER_SIZE);

        Session *s;
        s = (Session *)malloc(sizeof(Session));
        s->dir = strdup(user_dir);
        sess[client] = *s;

        pwd_cmd(client, true);
    }
}

// Function to List files in current server directory
void list_cmd(int client)
{
    send(client, ret_response(150), BUFFER_SIZE, 0);
    int channel = data_channel( S_DATAPORT, sess[client].currentDataPort);

    int count = 0;
    struct dirent *dir;

    chdir(sess[client].dir);

    DIR *dirr;
    dirr = opendir(".");
    if (dirr)
    {
        while ((dir = readdir(dirr)) != NULL)
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
        closedir(dirr);
    }

    close(channel);
}

// Function to Retrieve File from server directory
void retr_cmd(int client, char *fn)
{
    send(client, ret_response(150), BUFFER_SIZE, 0);

    int channel = data_channel( S_DATAPORT, sess[client].currentDataPort);

    FILE *file;
    file = fopen(fn, "rb");

    if (file == NULL)
    {
        printf("Can't open %s\n", fn);
    }
    else
    {
        struct stat stat_buf;
        int temp;
        int rc = stat(fn, &stat_buf);
        int fsize = stat_buf.st_size;

        char databuff[fsize + 1];
        fread(databuff, 1, sizeof(databuff), file);
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
        fclose(file);
    }
    close(channel);
    send(client, ret_response(226), BUFFER_SIZE, 0);
}

// Function to store File into server directory
void stor_cmd(int client, char *fn)
{
    send(client, ret_response(150), BUFFER_SIZE, 0);
    usleep(1000000);
    int channel = data_channel( S_DATAPORT, sess[client].currentDataPort);

    char reader[BUFFER_SIZE];
    bzero(reader, BUFFER_SIZE);

    FILE *file;
    file = fopen(fn, "wb");

    int total = 0, ln;

    while ((ln = read(channel, reader, BUFFER_SIZE)) > 0)
    {
        fwrite(reader, 1, BUFFER_SIZE, file);
        total += ln;
        if (ln < BUFFER_SIZE)
        {
            break;
        }
    }
    fclose(file);
    close(channel);
    send(client, ret_response(226), BUFFER_SIZE, 0);
}

// Function to Check Username
void user_cmd(int client, char *buffer)
{
    if (session[client] == 1 || session[client] == 0)
    {
        send(client, ret_response(503), BUFFER_SIZE, 0);
        return;
    }

    char *user_name = buffer + 5;
    for (int i = 0; i < arr_size - 1; i++)
    {
        if (strcmp(user_name, data[i].user) == 0)
        {
            session[client] = 0;
            send(client, ret_response(331), BUFFER_SIZE, 0);
            bzero(check_username, sizeof(check_username));
            memcpy(check_username, user_name, strlen(user_name));
            break;
        }
    }

    if (session[client] == -1)
    {
        send(client, ret_response(530), BUFFER_SIZE, 0);
    }
}

// Function to Check password
void pass_cmd(int client, char *buffer)
{
    if (session[client] == 1 || session[client] == -1)
    {
        send(client, ret_response(503), BUFFER_SIZE, 0);
        return;
    }

    char *pass = buffer + 5;
    if (session[client] == 0)
    {
        for (int i = 0; i < arr_size - 1; i++)
        {
            if (strcmp(pass, data[i].pass) == 0 && strcmp(check_username, data[i].user) == 0)
            {
                session[client] = 1;
                send(client, ret_response(230), BUFFER_SIZE, 0);
                break;
            }
        }
        if (session[client] == 0)
        {
            session[client] = -1;
            send(client, ret_response(530), BUFFER_SIZE, 0);
        }
    }
}


// Function that handles client input
void main_commands(int client, char *buffer)
{
    char all_commands[8][6] = {"USER", "PASS", "PWD", "CWD", "PORT", "LIST", "RETR", "STOR"};
    char command[5];
    strncpy(command, buffer + 0, 4);
    printf("Command: %s\n", buffer);

    if (strcmp(command, all_commands[0]) == 0)
    {
        user_cmd(client, buffer);
        return;
    }
    else if (strcmp(command, all_commands[1]) == 0)
    {
        pass_cmd(client, buffer);
        return;
    }

    if (autheticate_user(client, command))
    {
        if (strcmp(command, all_commands[2]) == 0)
        {
            pwd_cmd(client, false);
        }
        else if (strstr(command, all_commands[3]))
        {
            cwd_cmd(client, buffer);
        }
        else if (strstr(command, all_commands[4]))
        {
            port_cmd(client, buffer);
        }
        else if (strcmp(command, all_commands[5]) == 0)
        {
            int pid = fork();
            if (pid == 0)
            {
                list_cmd(client);
                exit(EXIT_SUCCESS);
            }
        }
        else if (strcmp(command, all_commands[6]) == 0)
        {
            int pid = fork();
            if (pid == 0)
            {
                retr_cmd(client, buffer + 5);
                exit(EXIT_SUCCESS);
            }
        }
        else if (strcmp(command, all_commands[7]) == 0)
        {
            int pid = fork();
            if (pid == 0)
            {
                stor_cmd(client, buffer + 5);
                exit(EXIT_SUCCESS);
            }
        }
        else
        {
            send(client, ret_response(202), BUFFER_SIZE, 0);
        }
    }
    else if (strstr(command, all_commands[2]) || strstr(command, all_commands[3]) || strstr(command, all_commands[5]) || strstr(command, all_commands[6]) || strstr(command, all_commands[7]))
    {
        send(client, ret_response(530), BUFFER_SIZE, 0);
    }
    else
    {
        send(client, ret_response(202), BUFFER_SIZE, 0);
    }
}

// Main Function
int main()
{

    char *users_filename = "users.txt";
    arr_size = read_users_file(users_filename);
    char users_dir[256];
    getcwd(users_dir, 256);
    Session *s; // Not sure how to change this Session thing
    int idx = 0;
    while (idx < 100){
        session[idx] = -1;
        s = (Session *)malloc(sizeof(Session));
        s->dir = strdup(users_dir);
        sess[idx] = *s;

    }

    int control_socket = control_channel(S_CONTROLPORT, 0); // make two different functions one control channel, data channel

    // DECLARE 2 fd sets (file descriptor sets : a collection of file descriptors)
    fd_set all_sockets;
    fd_set ready_sockets;

    // zero out/iniitalize our set of all sockets
    FD_ZERO(&all_sockets);

    // adds one socket (the current socket) to the fd set of all sockets
    FD_SET(control_socket, &all_sockets);

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
                if (fd == control_socket)
                {
                    // accept that new connection
                    int client_sd = accept(control_socket, 0, 0);

                    // add the newly accepted socket to the set of all sockets that we are watching
                    FD_SET(client_sd, &all_sockets);

                    session[fd] = -1;
                }
                else
                {
                    char buffer[BUFFER_SIZE];
                    bzero(buffer, sizeof(buffer));

                    int bytes = read(fd, buffer, sizeof(buffer));
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
                    main_commands(fd, buffer);
                }
            }
        }
    }

    // close
    close(control_socket);
    return 0;
}