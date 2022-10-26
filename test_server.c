#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
  // opens TCP socket
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);

  // verifies that the socket is indeed opened
  if (server_socket < 0) {
    perror("socket");
    return 1;
  }

	// setsockopt to "avoid binding issues"
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

  // creates structure for control channel (port 21)
  struct sockaddr_in server_control;
  // initializes structure to zero
  memset(&server_control, 0, sizeof(server_control));
  // sets protocol family
	server_control.sin_family = AF_INET;
  // listens on port 21 (for control connection)
	server_control.sin_port = htons(21);
  // uses localhost IP address
	server_control.sin_addr.s_addr = inet_addr("127.0.0.1");

  // tries to bind socket
  if (bind(server_socket, (struct sockaddr*) &server_control, sizeof(server_control)) < 0) {
    perror("binding socket (use sudo)");
    return 1;
  }

	// tries to listen (using the usual parameter `5` as the number of active participants)
  if (listen(server_socket, 5) < 0) {
    perror("listening");
		close(server_socket); // frees up port before exiting
    return 1;

  } else {
		// if listening successful
    printf("Server is listening. Clients may now connect!\n");
	}

  // creates buffer for control/command exchange and initializes to zero
  char msg_buffer[100];
  memset(msg_buffer, 0, 100);

	// creates structure to store client address
	struct sockaddr_in new_cl_control;
	// initializes structure to zero
	memset(&new_cl_control, 0, sizeof(new_cl_control));
	// and obtains size of the structure
	socklen_t cl_addr_size = sizeof(struct sockaddr_in);

	// creates buffer to store client address and initializes to zero
  char cl_buffer[20];
  memset(cl_buffer, 0, 20);

  while (1) {
		// keeps accepting new client connection
		int new_client = accept(server_socket, (struct sockaddr*) &new_cl_control, &cl_addr_size);

		// makes sure that the connection is indeed accepted
		if (new_client < 0) {
			// if unsuccessful
			perror("accepting");
			close(server_socket); // frees up port before exiting
	    return 1;
		}

		// reads client's address
		strcpy(cl_buffer, inet_ntoa(new_cl_control.sin_addr));
		// and port number (network-to-host converted)
		int cl_port = ntohs(new_cl_control.sin_port);

		sprintf(msg_buffer, "%d", cl_port);

		write(new_client, msg_buffer, 100);

		// prints IP address and port number of the newly connected client
		printf("[%s:%d] Connected\n", cl_buffer, cl_port);

		// spawns child process using fork() to deal with this new client
		if (fork() == 0) {

			while (1) {
				// using same buffer (firstly set to zero, again and again)
				memset(msg_buffer, 0, 100);

				// tries to receive message from client using `read`
		    if (read(new_client, msg_buffer, 100) < 0) {
		      // if receiving message unsuccessful
		      perror("Failed to receive message");
		      printf("Try again...\n");
		      continue;
		    }

				// displays message received
				if (strlen(msg_buffer) == 0) {
					// if the message is of length zero
					printf("[%s:%d] Received message of length 0.\n", cl_buffer, cl_port);

				} else {
					// if the message is non-trivial
					printf("[%s:%d] Received message: %s\n", cl_buffer, cl_port, msg_buffer);
				}

				// after receiving message from client:
		    // closes the socket, if the client enters "BYE!" or message is of length zero
		    if ((strcmp(msg_buffer, "BYE!") == 0) || (strlen(msg_buffer) == 0))  {
					// prints message of disconnection
					printf("[%s:%d] Disconnected\n", cl_buffer, cl_port);
		      close(new_client);
					// child process will also terminate after the while loop breaks
		      break;
		    }

				// otherwise: tries to send message back to client using `write`
		    if (write(new_client, msg_buffer, 100) < 0) {
		      // if sending data unsuccessful
		      perror("Failed to send message");
		      printf("Try again...\n");
		      continue;
		    }

				// displays message sent
		    printf("[%s:%d] Sent message: %s\n", cl_buffer, cl_port, msg_buffer);

			}
		}
  }
  return 0;
}
