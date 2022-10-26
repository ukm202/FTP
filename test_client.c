#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

int data_channel(unsigned short control_num, unsigned short plus_i, int s_id) {
	// opens new TCP socket for data connection
	int data_id = socket(AF_INET, SOCK_STREAM, 0);

	// verifies that the socket is indeed opened
  if (data_id < 0) {
    perror("data socket");
    return -1;
  }

	// setsockopt to "avoid binding issues"
	setsockopt(data_id, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

	// computes the exact value of `port (N + i)` for data connection
	unsigned short N_plus_i = control_num + plus_i;

	// creates structure for data channel (port (N + i))
  struct sockaddr_in data_control;
  // initializes structure to zero
  memset(&data_control, 0, sizeof(data_control));
  // sets protocol family
	data_control.sin_family = AF_INET;
  // connects to server on port (N + i)
	data_control.sin_port = htons(N_plus_i);
  // uses localhost IP address
	data_control.sin_addr.s_addr = inet_addr("127.0.0.1");

	// tries to bind data connection socket
  if (bind(data_id, (struct sockaddr*) &data_control, sizeof(data_control)) < 0) {
    perror("binding data socket");
    return -1;
  }

	// tries to listen
  if (listen(data_id, 1) < 0) {
    perror("listening");
		close(data_id); // frees up port before exiting
    return -1;

  } else {
		// if listening successful
    printf("Client opened new data connection on port %d.\n", N_plus_i);
	}

	// "accepts" server's connection to port (N + i)
	int server_data = accept(data_id, 0, 0);

	// makes sure that the connection is indeed accepted
	if (server_data < 0) {
		// if unsuccessful
		perror("accepting");
		close(data_id); // frees up port before exiting
		return -1;
	}

	return 0;
}

int main() {
  // opens TCP socket
	int socket_id = socket(AF_INET, SOCK_STREAM, 0);

	// keeps track of the original port # N from which clients connects for control
	// as well as the data connection "increment", i
	unsigned short client_N;
	unsigned short inc_i = 1;

  // verifies that the socket is indeed opened
  if (socket_id < 0) {
    perror("socket");
    return 1;
  }

  // creates structure for control channel (port 21)
  struct sockaddr_in cl_control;
  // initializes structure to zero
  memset(&cl_control, 0, sizeof(cl_control));
  // sets protocol family
	cl_control.sin_family = AF_INET;
  // connects to server on port 21
	cl_control.sin_port = htons(21);
  // uses localhost IP address
	cl_control.sin_addr.s_addr = inet_addr("127.0.0.1");

  // tries to connect to server (control channel)
  if (connect(socket_id, (struct sockaddr*) &cl_control, sizeof(cl_control)) < 0) {
    perror("control connection");
    return 1;
  } else {
    // if connection successful
    printf("Control connected to server!\n");
  }

  // creates buffer for control/command exchange and initializes to zero
  char control_buffer[100];
  memset(control_buffer, 0, 100);

	// tries to identify client's `Port N` from server using `read`
	if (read(socket_id, control_buffer, 100) < 0) {
		// if receiving unsuccessful
		perror("client N");
		return 1;
	}
	// otherwise, store client's `Port N` for future data connections
	client_N = (unsigned short) atoi(control_buffer);

	printf("Client N is: %d\n", client_N);

  while (1) {
    // prompts user to enter a command and read it
    printf("ftp> ");
    fgets(control_buffer, 100, stdin);
    // removes the newline character also read from the user
    control_buffer[strlen(control_buffer) - 1] = '\0';

		// firstly: send command to server
		// tries to send user's message to server using `write`
		if (write(socket_id, control_buffer, strlen(control_buffer)) < 0) {
			// if sending data unsuccessful
			perror("sending control");
			return 1;
		}

		// take actions based on command input
		if ((strcmp(control_buffer, "RETR") == 0) || (strcmp(control_buffer, "STOR") == 0) || (strcmp(control_buffer, "LIST") == 0)) {
			if (data_channel(client_N, inc_i, socket_id) < 0) {
				return 1;
			} else {
				inc_i++;
			}
		} else if (strcmp(control_buffer, "QUIT") == 0) {
			// closes the socket and terminates, if the user enters "QUIT"
			close(socket_id);
      return 0;

		} else if (strlen(control_buffer) == 0) {
      // or if the message is of length zero
      printf("Message is of length 0. Disconnected from server.\n");
      close(socket_id);
      return 0;
    }

    // otherwise: receive response from server
    // using same buffer (firstly set to zero again)
    memset(control_buffer, 0, 100);

    // tries to receive response from server using `read`
    if (read(socket_id, control_buffer, 100) < 0) {
      // if receiving response unsuccessful
      perror("Failed to receive response");
      printf("Try again...\n");
      continue;
    }

    // displays message received
    printf("Server response: %s\n", control_buffer);

  }
  return 0;
}
