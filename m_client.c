#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

// global variables to specify (random new) port range
#define PORT_MIN_VAL 1024
#define PORT_MAX_VAL 9999
// as well as the control msg/command exchange buffer size
#define BUFFER 2048

// global boolean variables to account for login state
bool pass = false;
bool login = false;

int new_rand_port() {
	// generate a new random port (for data connection)
	// based on min and max values specified in global declaration
	srand(time(NULL));
	return (rand() % (PORT_MAX_VAL + 1 - PORT_MIN_VAL)) + PORT_MIN_VAL;
}

int create_control() {
	// creates control connection with server on port 21

	// opens TCP socket for FTP control connection
	int control_socket = socket(AF_INET, SOCK_STREAM, 0);

  // verifies that the socket is indeed opened
  if (control_socket < 0) {
    perror("control socket");
    return -1;
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
    return -1;
  }

	return control_socket;
}

int control_exchange(int control_conn, char* msg_exchange) {
	// firstly: send client's command/message to server using `write`
	if (write(control_conn, msg_exchange, strlen(msg_exchange)) < 0) {
		// if sending data unsuccessful
		perror("sending control");
		return -1;
	}

	// creates temporary buffer for receiving message from server
	char temp_receive[BUFFER];
  memset(temp_receive, 0, BUFFER);

	// then: tries to receive response from server using `read`
	if (read(control_conn, temp_receive, BUFFER) < 0) {
		// if receiving response unsuccessful
		perror("receiving from server");
		return -1;

	}

	// if receiving response successful: print message received from server
	printf("%s\n", temp_receive);

	// (another) temp buffer to isolate and store status code
	char temp_stat[4];
  memset(temp_stat, 0, 4);
	// copy first three chars of received message
	// (i.e., the status code) over
	strncpy(temp_stat, temp_receive, 3);

	// convert to integer data type and return
	int status_code = atoi(temp_stat);
	return status_code;
}

int create_data(int control_conn, int data_port, char* msg_exchange) {
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
	int ip_len = strlen(cl_buffer);
	for (int i = 0; i < ip_len; i++) {
		if (cl_buffer[i] == '.') {
			cl_buffer[i] = ',';
		}
	}

	// from project instruction (Section 4)
	// convert port to p1 and p2
	int p1, p2;
	p1 = cl_port / 256;
	p2 = cl_port % 256;

	// generate "PORT h1,h2,h3,h4,p1,p2" message
	sprintf(port_buffer, "PORT %s,%d,%d", cl_buffer, p1, p2);

	// send PORT command to server, receives and compares status code
	int port_code = control_exchange(control_conn, port_buffer);
	if (port_code != 200) {
		// if not received expected status code
		perror("PORT command");
		close(data_id); // frees up port before exiting
		return -1;
	}

	// then: send user command to server, receives and compares status code
	int data_code = control_exchange(control_conn, msg_exchange);
	if (data_code != 150) {
		// if not received expected status code
		perror("data connection");
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


void process_command(int control_conn, char* user_command, char* msg_exchange) {
	// take actions based on command input
	// firstly: handle local (server) commands/cases
	if (strstr(user_command, "QUIT")) {
		// closes the socket and terminates, if the user enters "QUIT"
		printf("221 Service closing control connection.\n");
		close(control_conn);
		exit(0);

	} else if (strlen(msg_exchange) == 0) {
		// or if the message is of length zero
		printf("221 Service closing control connection.\n");
		close(control_conn);
		exit(0);

	} else if (strstr(user_command, "!LIST")) {
		// (local command) lists all the files under the current client directory
		// instantiate directory structure
		struct dirent* directory_structure;
		// and variable to skip ".", "..", and ".DS_Store"
		int dir_offset = 0;

		DIR* local_dir = opendir(".");

		if (local_dir) {
			// keep reading directory
			while ((directory_structure = readdir(local_dir)) != NULL) {
				if (dir_offset > 2) {
					// skips ".", "..", and ".DS_Store" at the start of the directory
					printf("%s\n", directory_structure->d_name);
				}
				dir_offset++;
			}
			closedir(local_dir);
		}

	} else if (strstr(user_command, "!CWD")) {
		// (local command) changes the current client directory
		char* temp_dir = msg_exchange + 5;
		if (chdir(temp_dir) < 0) {
			// if invalid directory (or other error)
			printf("550 No such file or directory.\n");
			return;
		}
		// prints updated working directory
		printf("Working directory changed to:\n");
		// creates buffer to store working directory
	  char temp_wd[100];
	  memset(temp_wd, 0, 100);
		if (getcwd(temp_wd, sizeof(temp_wd)) == NULL) {
			// if error with getcwd
			printf("Error with getting current working directory.\n");
			return;
		}
		// if successful: print current working directory
		printf("%s\n", temp_wd);

	} else if (strstr(user_command, "!PWD")) {
		// (local command) displays the current client directory
		// creates buffer to store working directory
	  char temp_wd[100];
	  memset(temp_wd, 0, 100);
		if (getcwd(temp_wd, sizeof(temp_wd)) == NULL) {
			// if error with getcwd
			printf("Error with getting current working directory.\n");
			return;
		}
		// if successful: print current working directory
		printf("%s\n", temp_wd);

	} else {
		// for other (non-local) commands that necessitate communication with server
		if ((!pass) && (!login) && (strstr(user_command, "USER"))) {
			// if username not yet entered
			int stat_code = control_exchange(control_conn, msg_exchange);
			if (stat_code == 331) {
				// if `331 Username OK, need password`
				// update relevant global boolean variables
				pass = true;
				login = false;
			}

		} else if ((pass) && (!login) && (strstr(user_command, "PASS"))) {
			// if username already entered, and password not yet entered
			int stat_code = control_exchange(control_conn, msg_exchange);
			if (stat_code == 230) {
				// if `230 User logged in, proceed`
				// update relevant global boolean variables
				pass = true;
				login = true;
			}

		} else if ((pass) && (login)) {
			// if user successfully logged in
			if (strstr(user_command, "STOR")) {
				// upload a local file named filename from the current client directory to the current server directory
				// spawns child process using fork() to deal with concurrency
				if (fork() == 0) {
					// opens (random) new port for data connection
					int new_data_port = new_rand_port();
					// obtains socket descriptor (from server's data connection)
					int data_conn = create_data(control_conn, new_data_port, msg_exchange);

					// obtains file name based on user input
					char* temp_fname = msg_exchange + 5;
					// verifies that the file can indeed be opened (read binary)
					FILE* file_ptr;
					if ((file_ptr = fopen(temp_fname, "rb")) == NULL) {
						printf("550 No such file or directory.\n");
						return;
					}

					// structure to store file information
					struct stat file_info;
					// tries to get file stat (file size)
					if (stat(temp_fname, &file_info) < 0) {
						perror("getting fstat");
						return;
					}
					int file_size = file_info.st_size;

					// dynamically allocate array to store file (plus one for EOF char)
					char* file_buffer = (char*) malloc(file_size + 1);
					// read file into `file_buffer`, one byte at a time (read binary)
					fread(file_buffer, 1, (file_size + 1), file_ptr);

					// variables to keep track of file uploading progress
					int stor_so_far = 0;
					int stor_left = file_size;
					int each_time;

					while (stor_so_far < file_size) {
						char* file_offset = file_buffer + stor_so_far;
						each_time = write(data_conn, file_offset, stor_left);

						if (each_time < 0) {
							printf("Error writing file %s to server.\n", temp_fname);
							break;
						}

						stor_so_far += each_time;
						stor_left -= each_time;
					}

					// at the end of file transfer: closes the file pointer
					fclose(file_ptr);
					// frees previous malloc
			    free(file_buffer);
					// closes data connection
					close(data_conn);

					// also: receive server message (from control connection)
					memset(msg_exchange, 0, BUFFER);
					// tries to receive response from server using `read`
					if (read(control_conn, msg_exchange, BUFFER) < 0) {
						// if receiving response unsuccessful
						perror("receiving from server");
						return;
					} else {
						// if receiving response successful: print message received from server
						printf("%s\n", msg_exchange);
					}
				}

			} else if (strstr(user_command, "RETR")) {
				// download a file named filename from the current server directory to the current client directory
				// spawns child process using fork() to deal with concurrency
				if (fork() == 0) {
					// opens (random) new port for data connection
					int new_data_port = new_rand_port();
					// obtains socket descriptor (from server's data connection)
					int data_conn = create_data(control_conn, new_data_port, msg_exchange);

					// obtains file name based on user input
					char temp_fname[100];
				  memset(temp_fname, 0, 100);
					char* temp_fname_ptr = msg_exchange + 5;
					strcpy(temp_fname, temp_fname_ptr);
					
					// verifies that the file can indeed be opened (write binary)
					FILE* file_ptr;
					if ((file_ptr = fopen(temp_fname, "wb")) == NULL) {
						perror("writing file");
						return;
					}

					memset(msg_exchange, 0, BUFFER);

					// variables to keep track of file downloading progress
					int stor_so_far = 0;
					int each_time;

					while (1) {
						each_time = read(data_conn, msg_exchange, BUFFER);

						if (each_time < 0) {
							printf("Error downloading file %s from server.\n", temp_fname);
							break;
						}
						// write information read each time to file
						fwrite(msg_exchange, 1, BUFFER, file_ptr);
						stor_so_far += each_time;

						// if file downloading completed
						if (each_time < BUFFER) {
							break;
						}
					}

					printf("%d bytes received for file %s\n", stor_so_far, temp_fname);

					// at the end of file transfer: closes the file pointer
					fclose(file_ptr);
					// closes data connection
					close(data_conn);

					// also: receive server message (from control connection)
					memset(msg_exchange, 0, BUFFER);
					// tries to receive response from server using `read`
					if (read(control_conn, msg_exchange, BUFFER) < 0) {
						// if receiving response unsuccessful
						perror("receiving from server");
						return;
					} else {
						// if receiving response successful: print message received from server
						printf("%s\n", msg_exchange);
					}
				}

			} else if (strstr(user_command, "LIST")) {
				// list all the files under the current server directory
				// spawns child process using fork() to deal with concurrency
				if (fork() == 0) {
					// opens (random) new port for data connection
					int new_data_port = new_rand_port();
					// obtains socket descriptor (from server's data connection)
					int data_conn = create_data(control_conn, new_data_port, msg_exchange);

					// variables to keep track of progress
					int each_time;

					// keep receiving new data (lines) from server
					while (1) {
						memset(msg_exchange, 0, BUFFER);

						each_time = read(data_conn, msg_exchange, BUFFER);

						if (each_time < 0) {
							close(data_conn);
							perror("receiving from server");
							return;
						} else if (each_time == 0) {
							// no more messages sent from server
							close(data_conn);
							break;
						}

						printf("%s\n", msg_exchange);
					}

					// closes data connection
					close(data_conn);
				}
			} else {
				// for all other commands: send to server and let server handle them
				control_exchange(control_conn, msg_exchange);
			}
		} else {
			// for all other commands: send to server and let server handle them
			control_exchange(control_conn, msg_exchange);
		}
	}
}


int main() {
	// creates control connection with server on port 21
	int control_socket = create_control();
	// verifies that the control connection is indeed opened
	if (control_socket < 0) {
		perror("control channel");
    return -1;
  }

  // creates buffers for control command exchange and initializes to zero
  char control_buffer[BUFFER];
  memset(control_buffer, 0, BUFFER);

	// also buffer to isolate and store the command
	char control_command[8];
	memset(control_command, 0, 8);

  while (1) {
    // prompts user to enter a command and read it
    printf("ftp> ");
    fgets(control_buffer, BUFFER, stdin);
    // removes the newline character also read from the user
    control_buffer[strlen(control_buffer) - 1] = '\0';

		// isolate and identify the command (at most 5 chars based on instructions)
    strncpy(control_command, control_buffer, 5);

		// process user command based on message and command entered
		process_command(control_socket, control_command, control_buffer);

		// set all buffers to zero again before next exchange
    memset(control_buffer, 0, BUFFER);
		memset(control_command, 0, 8);
  }

  return 0;
}
