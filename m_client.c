#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

// global boolean variables to account for login state
bool pass = false;
bool login = false;

int new_rand_port(int min_val, int max_val) {
	// generate a new random port (for data connection)
	// based on min and max values specified in arguments
	srand(time(NULL));
	return (rand() % (max_val + 1 - min_val)) + min_val;
}

int get_status_code(int control_conn, char* send_msg, char* receive_msg) {
	// firstly: send client's command/message to server using `write`
	if (write(control_conn, send_msg, strlen(send_msg)) < 0) {
		// if sending data unsuccessful
		perror("sending control");
		return -1;
	}

	// then: tries to receive response from server using `read`
	if (read(control_conn, receive_msg, 100) < 0) {
		// if receiving response unsuccessful
		perror("receiving from server");
		return -1;

	} else {
		// if receiving response successful: print message received from server
		printf("%s\n", receive_msg);
	}

	// temp buffer to isolate and store status code
	char temp_stat[4];
  memset(temp_stat, 0, 4);
	// copy first three chars of received message
	// (i.e., the status code) over
	strncpy(temp_stat, receive_msg, 3);

	// convert to integer data type and return
	int status_code = atoi(temp_stat);
	return status_code;
}

int data_channel(int control_conn, int data_port, int req_status, char* send_msg, char* receive_msg) {
	// opens new TCP socket for data connection
	int data_id = socket(AF_INET, SOCK_STREAM, 0);

	// verifies that the socket is indeed opened
  if (data_id < 0) {
    perror("new data socket");
    return -1;
  }

	// setsockopt to "avoid binding issues"
	setsockopt(data_id, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

	// creates structure for data channel
  struct sockaddr_in data_control;
  // initializes structure to zero
  memset(&data_control, 0, sizeof(data_control));
  // sets protocol family
	data_control.sin_family = AF_INET;
  // connects to server on port `data_port`
	data_control.sin_port = htons(data_port);
  // not binding to any specific IP address
	data_control.sin_addr.s_addr = htonl(INADDR_ANY);

	// tries to bind data connection socket
  if (bind(data_id, (struct sockaddr*) &data_control, sizeof(data_control)) < 0) {
    perror("binding data socket");
    return -1;
  }

	// tries to listen (using the usual parameter `5` as the number of active participants)
  if (listen(data_id, 5) < 0) {
    perror("listening");
		close(data_id); // frees up port before exiting
    return -1;
  }

	// creates another structure to store client address
	struct sockaddr_in cl_address;
	// initializes structure to zero
	memset(&cl_address, 0, sizeof(cl_address));
	// and obtains size of the structure
	socklen_t cl_addr_size = sizeof(struct sockaddr_in);

	// tries to get name for current socket (data connection)
  if (getsockname(data_id, (struct sockaddr*) &cl_address, &cl_addr_size) < 0) {
    perror("getsockname");
		close(data_id); // frees up port before exiting
    return -1;
  }

	// creates buffer to store client address and initializes to zero
  char cl_buffer[20];
  memset(cl_buffer, 0, 20);

	// reads client's address
	strcpy(cl_buffer, inet_ntoa(cl_address.sin_addr));
	// and port number (network-to-host converted)
	int cl_port = ntohs(cl_address.sin_port);

	// creates another buffer for formatting PORT command
  char port_buffer[100];
  memset(port_buffer, 0, 100);

	// replace all occurrences of '.' in IP address with ','
	char* c_pos;
	while ((c_pos = strchr(cl_buffer, '.'))) {
		*c_pos = ',';
		c_pos = strchr(c_pos, '.');
	}

	// from project instruction (Section 4)
	// convert port to p1 and p2
	int p1, p2;
	p1 = cl_port / 256;
	p2 = cl_port % 256;

	// generate "PORT h1,h2,h3,h4,p1,p2" message
	sprintf(port_buffer, "PORT %s,%d,%d", cl_buffer, p1, p2);

	// send PORT command to server, receives and compares status code
	int data_stat_code = get_status_code(control_conn, send_msg, receive_msg);
	if (data_stat_code != req_status) {
		// if not received expected status code
		perror("data PORT status");
		close(data_id); // frees up port before exiting
		return -1;
	}

	// "accepts" server's connection to port
	int server_data = accept(data_id, 0, 0);

	// makes sure that the connection is indeed accepted
	if (server_data < 0) {
		// if unsuccessful
		perror("accepting");
		close(data_id); // frees up port before exiting
		return -1;
	}

	return server_data;
}

int main() {
  // opens TCP socket for FTP control connection
	int control_socket = socket(AF_INET, SOCK_STREAM, 0);

  // verifies that the socket is indeed opened
  if (control_socket < 0) {
    perror("control socket");
    return 1;
  }

	// setsockopt to "avoid binding issues"
	setsockopt(control_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

  // creates structure for control channel (port 21)
  struct sockaddr_in cl_control;
  // initializes structure to zero
  memset(&cl_control, 0, sizeof(cl_control));
  // sets protocol family
	cl_control.sin_family = AF_INET;
  // connects to server on port 21 (control channel)
	cl_control.sin_port = htons(21);
	// not binding to any specific IP address
	cl_control.sin_addr.s_addr = htonl(INADDR_ANY);

  // tries to connect to server (control channel)
  if (connect(control_socket, (struct sockaddr*) &cl_control, sizeof(cl_control)) < 0) {
		perror("control connection");
    return 1;
  }

  // creates buffers for control command exchange and initializes to zero
  char send_control[100];
  memset(send_control, 0, 100);
	char receive_control[100];
  memset(receive_control, 0, 100);

	// also buffer to isolate and store the command
	char control_command[8];
	memset(control_command, 0, 8);

	int stat_code, data_conn, new_data_port;

  while (1) {
		// set all buffers to zero again before exchange
    memset(send_control, 0, 100);
		memset(receive_control, 0, 100);
		memset(control_command, 0, 8);

    // prompts user to enter a command and read it
    printf("ftp> ");
    fgets(send_control, 100, stdin);
    // removes the newline character also read from the user
    send_control[strlen(send_control) - 1] = '\0';

		// isolate and identify the command (at most 5 chars based on instructions)
    strncpy(control_command, send_control, 5);

		// take actions based on command input
		// firstly: handle local (server) commands/cases
		if (strcmp(control_command, "QUIT") == 0) {
			// closes the socket and terminates, if the user enters "QUIT"
			printf("221 Service closing control connection.\n");
			close(control_socket);
      return 0;

		} else if (strlen(send_control) == 0) {
      // or if the message is of length zero
      printf("Message is of length 0.\n221 Service closing control connection.\n");
      close(control_socket);
      return 0;

    } else if (strcmp(control_command, "!LIST") == 0) {
			// (local command) lists all the files under the current client directory
			system("ls -l");

		} else if (strcmp(control_command, "!CWD") == 0) {
			// (local command) changes the current client directory
			char* temp_dir = send_control + 5;
			if (chdir(temp_dir) < 0) {
				// if invalid directory (or other error)
				printf("550 No such file or directory.\n");
				continue;
			}
			// prints updated working direcoty
			system("pwd");

		} else if (strcmp(control_command, "!PWD") == 0) {
			// (local command) displays the current client directory
			system("pwd");

		} else {
			// for other (non-local) commands that necessitate communication with server
			if ((strcmp(control_command, "USER") == 0) && (!pass) && (!login)) {
				// if username not yet entered
				stat_code = get_status_code(control_socket, send_control, receive_control);
				if (stat_code == 331) {
					// if `331 Username OK, need password`
					// update relevant global boolean variables
					pass = true;
					login = false;
				}

			} else if ((strcmp(control_command, "PASS") == 0) && (pass) && (!login)) {
				// if username already entered, and password not yet entered
				stat_code = get_status_code(control_socket, send_control, receive_control);
				if (stat_code == 230) {
					// if `230 User logged in, proceed`
					// update relevant global boolean variables
					pass = true;
					login = true;
				}

			} else if ((pass) && (login)) {
				// if user successfully logged in

				if (strcmp(control_command, "STOR") == 0) {
					// upload a local file named filename from the current client directory to the current server directory
					// spawns child process using fork() to deal with concurrency
					if (fork() == 0) {
						// opens (random) new port for data connection
						new_data_port = new_rand_port(1024, 9999);
						// obtains socket descriptor (from server's data connection)
						data_conn = data_channel(control_socket, new_data_port, 150, send_control, receive_control);

						// obtains file name based on user input
						char* temp_fname = send_control + 5;
						// verifies that the file can indeed be opened (read binary)
					  FILE* file_ptr;
					  if ((file_ptr = fopen(temp_fname, "rb")) == NULL) {
							printf("550 No such file or directory.\n");
							continue;
					  }

						// structure to store file information
						struct stat file_info;

						// tries to get file stat (file size)
						if (stat(temp_fname, &file_info) < 0) {
							perror("getting fstat");
							continue;
						}
						int file_size = file_info.st_size;

						// dynamically allocate array to store file
						// plus one for EOF char
						char* file_buffer = (char*) malloc(file_size + 1);
						// read file into `file_buffer`, one byte at a time (read binary)
						fread(file_buffer, 1, (file_size + 1), file_ptr);


						// ----- TO BE CHANGED BELOW -----
						int total = 0, bytesleft = file_size, ln;
						while (total < file_size)
						{
								ln = write(data_conn, file_buffer + total, bytesleft);
								if (ln == -1)
								{
										printf("Error writing to socket.\n");
										break;
								}
								total += ln;
								bytesleft -= ln;
						}
						/*
						// firstly: send client's command/message to server using `write`
						if (write(control_conn, send_msg, strlen(send_msg)) < 0) {
							// if sending data unsuccessful
							perror("sending control");
							return -1;
						}
						*/
						// ----- TO BE CHANGED ABOVE -----


						// closes the file pointer
					  fclose(file_ptr);
						// closes data connection
						close(data_conn);

						// also: receive server message (from control connection)
						memset(receive_control, 0, 100);
						// tries to receive response from server using `read`
						if (read(control_socket, receive_control, 100) < 0) {
							// if receiving response unsuccessful
							perror("receiving from server");
							continue;

						} else {
							// if receiving response successful: print message received from server
							printf("%s\n", receive_control);
						}
					}
				} else if (strcmp(control_command, "RETR") == 0) {
					// download a file named filename from the current server directory to the current client directory
					// spawns child process using fork() to deal with concurrency
					if (fork() == 0) {
						// opens (random) new port for data connection
						new_data_port = new_rand_port(1024, 9999);
						// obtains socket descriptor (from server's data connection)
						data_conn = data_channel(control_socket, new_data_port, 150, send_control, receive_control);

						// obtains file name based on user input
						char* temp_fname = send_control + 5;
						// verifies that the file can indeed be opened (write binary)
					  FILE* file_ptr;
					  if ((file_ptr = fopen(temp_fname, "wb")) == NULL) {
							perror("writing file");
							continue;
					  }


						// ----- TO BE CHANGED BELOW -----
						char reader[1024];
						bzero(reader, 1024);

						int total = 0, ln;
						while ((ln = read(data_conn, reader, 1024)) > 0)
						{
								fwrite(reader, 1, 1024, file_ptr);
								total += ln;
								if (ln < 1024)
								{
										break;
								}
						}
						printf("Total data received for %s = %d bytes.\n", temp_fname, total);
						// ----- TO BE CHANGED ABOVE -----


						// closes the file pointer
					  fclose(file_ptr);
						// closes data connection
						close(data_conn);

						// also: receive server message (from control connection)
						memset(receive_control, 0, 100);
						// tries to receive response from server using `read`
						if (read(control_socket, receive_control, 100) < 0) {
							// if receiving response unsuccessful
							perror("receiving from server");
							continue;

						} else {
							// if receiving response successful: print message received from server
							printf("%s\n", receive_control);
						}
					}
				} else if (strcmp(control_command, "LIST") == 0) {
					// list all the files under the current server directory
					// spawns child process using fork() to deal with concurrency
					if (fork() == 0) {
						// opens (random) new port for data connection
						new_data_port = new_rand_port(1024, 9999);
						// obtains socket descriptor (from server's data connection)
						data_conn = data_channel(control_socket, new_data_port, 150, send_control, receive_control);


						// ----- TO BE CHANGED BELOW -----
						while (1)
						{
								memset(receive_control, 0, 100);
								int bytes = recv(data_conn, receive_control, 100, 0);
								if (bytes == 0) // client has closed the connection
								{
										close(data_conn);
										break;
								}

								printf("%s \n", receive_control);
						}
						// ----- TO BE CHANGED ABOVE -----


						// closes data connection
						close(data_conn);
					}
				} else {
					// for all other commands: send to server and let server handle them
					get_status_code(control_socket, send_control, receive_control);
				}
			} else {
				// for all other commands: send to server and let server handle them
				get_status_code(control_socket, send_control, receive_control);
			}
		}
  }
  return 0;
}
