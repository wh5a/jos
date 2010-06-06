#include <inc/lib.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

#define PORT 7

#define BUFFSIZE 32
#define MAXPENDING 5    // Max connection requests

static void
die(char *m)
{
	cprintf("%s\n", m);
	exit();
}

void 
handle_client(int sock)
{
	char buffer[BUFFSIZE];
	int received = -1;
	// Receive message
	if ((received = recv(sock, buffer, BUFFSIZE, 0)) < 0)
		die("Failed to receive initial bytes from client");

	// Send bytes and check for more incoming data in loop
	while (received > 0) {
		// Send back received data
		if (send(sock, buffer, received, 0) != received)
			die("Failed to send bytes to client");

		// Check for more data
		if ((received = recv(sock, buffer, BUFFSIZE, 0)) < 0)
			die("Failed to receive additional bytes from client");
	}
	closesocket(sock);
}

int 
umain(void)
{
	int serversock, clientsock;
	struct sockaddr_in echoserver, echoclient;
	char buffer[BUFFSIZE];
	unsigned int echolen;
	int received = 0;
	
	// Create the TCP socket
	if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		die("Failed to create socket");
	
	cprintf("opened socket\n");
	
	// Construct the server sockaddr_in structure
	memset(&echoserver, 0, sizeof(echoserver));       // Clear struct
	echoserver.sin_family = AF_INET;                  // Internet/IP
	echoserver.sin_addr.s_addr = htonl(INADDR_ANY);   // IP address
	echoserver.sin_port = htons(PORT);		  // server port
	
	cprintf("trying to bind\n");
	
	// Bind the server socket
	if (bind(serversock, (struct sockaddr *) &echoserver,
		 sizeof(echoserver)) < 0) 
	{
		die("Failed to bind the server socket");
	}

	// Listen on the server socket
	if (listen(serversock, MAXPENDING) < 0)
		die("Failed to listen on server socket");
	
	cprintf("bound\n");
	
	// Run until cancelled
	while (1) {
		unsigned int clientlen = sizeof(echoclient);
		// Wait for client connection
		if ((clientsock =
		     accept(serversock, (struct sockaddr *) &echoclient,
			    &clientlen)) < 0) 
		{
			die("Failed to accept client connection");
		}
		cprintf("Client connected: %s\n", inet_ntoa(echoclient.sin_addr));
		handle_client(clientsock);
	}
	
	closesocket(serversock);
	
	return 0;
}
