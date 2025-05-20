#include "main.h"


/*--------------------- VARIABLE - BEGIN --------------------*/
Connection_t device_connect[MAX_CONNECTIONS];
int connection_count = 0;
int server_sock = -1;
/*--------------------- VARIABLE - END   --------------------*/

/*--------------------- FUNCTION PROTOTYPE - BEGIN --------------------*/
void function_get_my_ip(char *ip);
void function_handle_cmd_myip();
void function_handle_cmd_help();
void function_handle_cmd_connect(char * token, int port);
void function_handle_cmd_list();
void function_handle_cmd_terminate(char * token);
void function_handle_cmd_send(char * token, int port);
void function_handle_cmd_exit();
void function_handle_command(char *cmd, int port);
void *function_handle_client(void *arg);
/*--------------------- FUNCTION PROTOTYPE - END   --------------------*/



/*------------------------- MAIN FUNCTION - BEGIN ---------------------*/
int main(int argc, char *argv[]) 
{
    fd_set read_fds;
    char input[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    int opt = 1;
    int max_fd, port, idx;
    pthread_t thread_id;

    if (argc != 2) 
    {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    port = atoi(argv[1]);
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) 
    {
        perror("Socket creation failed");
        return 1;
    }

    // use O_NONBLOCK for non-locking 
    fcntl(server_sock, F_SETFL, O_NONBLOCK);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // use SO_REUSEADDR to use immediately.
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
    {
        perror("Setsockopt failed");
        close(server_sock);
        return 1;
    }

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("Bind failed");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, MAX_CONNECTIONS) < 0) 
    {
        perror("Listen failed");
        close(server_sock);
        return 1;
    }

    printf("Chat server started on port %d\n", port);
    printf("Enter 'help' to see available commands\n");

    // connection list
    for (int i = 0; i < MAX_CONNECTIONS; i++) 
    {
        device_connect[i].sockfd = -1;
        device_connect[i].active = 0;
    }
    
    while (1) 
    {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); 
        FD_SET(server_sock, &read_fds);  

        max_fd = server_sock > STDIN_FILENO ? server_sock : STDIN_FILENO;
        for (int i = 0; i < MAX_CONNECTIONS; i++) 
        {
            if (device_connect[i].active) 
            {
                FD_SET(device_connect[i].sockfd, &read_fds);
                if (device_connect[i].sockfd > max_fd) 
                {
                    max_fd = device_connect[i].sockfd;
                }
            }
        }

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) 
        {
            perror("Select failed");
            continue;
        }

        // handle connect
        if (FD_ISSET(server_sock, &read_fds)) 
        {
            socklen_t client_len = sizeof(client_addr);
            int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
            if (client_sock < 0) continue;

            // socket client non-blocking
            fcntl(client_sock, F_SETFL, O_NONBLOCK);

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);

            char my_ip[INET_ADDRSTRLEN];
            function_get_my_ip(my_ip);
            if (strcmp(client_ip, my_ip) == 0 && client_port == port) 
            {
                printf("Error: Self-connection attempted\n");
                close(client_sock);
                continue;
            }

            for (int i = 0; i < MAX_CONNECTIONS; i++) 
            {
                if (device_connect[i].active && strcmp(device_connect[i].ip, client_ip) == 0 && device_connect[i].port == client_port) 
                {
                    printf("Error: Duplicate connection from %s:%d\n", client_ip, client_port);
                    close(client_sock);
                    continue;
                }
            }

            idx = -1;
            for (int i = 0; i < MAX_CONNECTIONS; i++) 
            {
                if (!device_connect[i].active) 
                {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) 
            {
                printf("Error: Maximum connections reached\n");
                close(client_sock);
                continue;
            }

            device_connect[idx].sockfd = client_sock;
            strcpy(device_connect[idx].ip, client_ip);
            device_connect[idx].port = client_port;
            device_connect[idx].active = 1;
            connection_count++;
            printf("New connection from %s:%d\n", client_ip, client_port);

            // for handle client message.
            int *client_sock_ptr = malloc(sizeof(int));
            *client_sock_ptr = client_sock;
            if (pthread_create(&thread_id, NULL, function_handle_client, client_sock_ptr) != 0) 
            {
                perror("Thread creation failed");
                close(client_sock);
                free(client_sock_ptr);
                continue;
            }
            pthread_detach(thread_id);
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) 
        {
            if (fgets(input, BUFFER_SIZE, stdin) == NULL) continue;
            input[strcspn(input, "\n")] = '\0'; // remove new line
            if (strlen(input) > 0) function_handle_command(input, port);
            printf("> "); // show '>'
            fflush(stdout);
        }

        // for other connection
        for (int i = 0; i < MAX_CONNECTIONS; i++) 
        {
            if (device_connect[i].active && FD_ISSET(device_connect[i].sockfd, &read_fds)) 
            {
                char buffer[BUFFER_SIZE];
                int bytes_received = recv(device_connect[i].sockfd, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received <= 0) 
                {
                    printf("Connection closed by peer %s:%d\n", device_connect[i].ip, device_connect[i].port);
                    close(device_connect[i].sockfd);
                    device_connect[i].active = 0;
                    connection_count--;
                    continue;
                }
                buffer[bytes_received] = '\0';
                printf("\n%s", buffer);
                printf("> ");
                fflush(stdout);
            }
        }
    }

    close(server_sock);
    return 0;
}

/*-------------------------- FUNCTION - BEGIN -------------------------*/
// get ip 
void function_get_my_ip(char *ip) 
{
    int sock;
    struct sockaddr_in my_server, local_addr;
    socklen_t addr_len;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) 
    {
        strcpy(ip, "Unknown");
        return;
    }

    my_server.sin_family = AF_INET;
    my_server.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &my_server.sin_addr);
    if (connect(sock, (struct sockaddr*)&my_server, sizeof(my_server)) < 0) 
    {
        strcpy(ip, "Unknown");
        close(sock);
        return;
    }

    addr_len = sizeof(local_addr);
    getsockname(sock, (struct sockaddr*)&local_addr, &addr_len);
    inet_ntop(AF_INET, &local_addr.sin_addr, ip, INET_ADDRSTRLEN);

    close(sock);
}

void function_handle_cmd_help()
{
	printf("Available commands:\n");
	printf("help                    - Display this help\n");
	printf("myip                    - Display this process's IP address\n");
	printf("myport                  - Display this process's listening port\n");
	printf("connect <ip> <port>     - Connect to another peer\n");
	printf("list                    - Display all connections\n");
	printf("terminate <id>          - Terminate a connection\n");
	printf("send <id> <message>     - Send message to a connection\n");
	printf("exit                    - Close all connections and exit\n");
}

void function_handle_cmd_myip()
{
	char ip[INET_ADDRSTRLEN];
	
	function_get_my_ip(ip);
	printf("My IP: %s\n", ip);
}

void function_handle_cmd_connect(char * token, int port)
{
	token = strtok(NULL, " ");
	if (token == NULL) 
	{
		printf("Error: Missing IP address\n");
		return;
	}
	
	char *dest_ip = token;
	
	token = strtok(NULL, " ");
	if (token == NULL) 
	{
		printf("Error: Missing port number\n");
		return;
	}
	
	int dest_port = atoi(token);
	char my_ip[INET_ADDRSTRLEN];
	
	function_get_my_ip(my_ip);
	if (strcmp(dest_ip, my_ip) == 0 && dest_port == port) 
	{
		printf("Error: Cannot connect to self\n");
		return;
	}

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) 
	{
		printf("Error: Failed to create socket\n");
		return;
	}

	struct sockaddr_in dest_addr;
	
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(dest_port);
	if (inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr) <= 0) 
	{
		printf("Error: Invalid IP address\n");
		close(sock);
		return;
	}

	if (connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) 
	{
		printf("Error: Failed to connect to %s:%d\n", dest_ip, dest_port);
		close(sock);
		return;
	}

	// check connection
	for (int i = 0; i < MAX_CONNECTIONS; i++) 
	{
		if (device_connect[i].active && strcmp(device_connect[i].ip, dest_ip) == 0 && device_connect[i].port == dest_port) 
		{
			printf("Error: Duplicate connection to %s:%d\n", dest_ip, dest_port);
			close(sock);
			return;
		}
	}

	// add connection
	int idx = -1;
	for (int i = 0; i < MAX_CONNECTIONS; i++) 
	{
		if (!device_connect[i].active) 
		{
			idx = i;
			break;
		}
	}
	if (idx == -1) 
	{
		printf("Error: Maximum connections reached\n");
		close(sock);
		return;
	}

	device_connect[idx].sockfd = sock;
	strcpy(device_connect[idx].ip, dest_ip);
	device_connect[idx].port = dest_port;
	device_connect[idx].active = 1;
	connection_count++;
	// show connection IP.
	printf("Connected to %s:%d\n", dest_ip, dest_port);
}

void function_handle_cmd_list()
{
	int display_id = 1;
	
	if (connection_count == 0) 
	{
		printf("No connections\n");
		return;
	}
	
	for (int i = 0; i < MAX_CONNECTIONS; i++) 
	{
		if (device_connect[i].active) {
			printf("%d: %s\n    %d\n", display_id++, device_connect[i].ip, device_connect[i].port);
		}
	}
}

void function_handle_cmd_terminate(char * token)
{
	int display_id = 1;
	int target_idx = -1;
	int id;
	
	token = strtok(NULL, " ");
	if (token == NULL) 
	{
		printf("Error: Missing connection ID\n");
		return;
	}
	
	id = atoi(token);
	for (int i = 0; i < MAX_CONNECTIONS; i++) 
	{
		if (device_connect[i].active) 
		{
			if (display_id == id) 
			{
				target_idx = i;
				break;
			}
			display_id++;
		}
	}
	
	if (target_idx == -1) 
	{
		printf("Error: Invalid connection ID\n");
		return;
	}
	
	close(device_connect[target_idx].sockfd);
	device_connect[target_idx].active = 0;
	connection_count--;
	printf("Connection %d terminated\n", id);
}

void function_handle_cmd_send(char * token, int port)
{
	int display_id = 1;
	int target_idx = -1;
	int id;
	char my_ip[INET_ADDRSTRLEN];
	char full_msg[BUFFER_SIZE];
	
	token = strtok(NULL, " ");
	if (token == NULL) 
	{
		printf("Error: Missing connection ID\n");
		return;
	}
	
	id = atoi(token);
	for (int i = 0; i < MAX_CONNECTIONS; i++) 
	{
		if (device_connect[i].active) 
		{
			if (display_id == id) 
			{
				target_idx = i;
				break;
			}
			display_id++;
		}
	}
	
	if (target_idx == -1) 
	{
		printf("Error: Invalid connection ID\n");
		return;
	}
	
	char *msg = strtok(NULL, "");
	
	if (msg == NULL || strlen(msg) > MAX_MESSAGE) 
	{
		printf("Error: Message too long or empty (max 100 chars)\n");
		return;
	}
	
	// Xóa ký tự newline nếu có
	msg[strcspn(msg, "\n")] = '\0';
	function_get_my_ip(my_ip);
	snprintf(full_msg, sizeof(full_msg), "Message received from %s\nSender's Port: %d\nMessage: %s\n", my_ip, port, msg);
	if (send(device_connect[target_idx].sockfd, full_msg, strlen(full_msg), 0) < 0) 
	{
		printf("Error: Failed to send message\n");
		return;
	}
	printf("Message sent to %d\n", id);
}

void function_handle_cmd_exit()
{
	for (int i = 0; i < MAX_CONNECTIONS; i++) 
	{
		if (device_connect[i].active) {
			close(device_connect[i].sockfd);
			device_connect[i].active = 0;
		}
	}
	connection_count = 0;
	if (server_sock != -1) 
	{
		close(server_sock);
	}
	printf("Exiting program\n");
	exit(0);
}

// handle each cmd
void function_handle_command(char *cmd, int port) 
{
    // prevent conflict data
    char cmd_copy[BUFFER_SIZE];
	
    strncpy(cmd_copy, cmd, BUFFER_SIZE - 1);
    cmd_copy[BUFFER_SIZE - 1] = '\0';
	
    char * token = strtok(cmd_copy, " ");

    if (token == NULL) 
    {
        printf("No command entered. Type 'help' for list of commands.\n");
        return;
    }

    if (strcmp(token, "help") == 0) 
    {
        function_handle_cmd_help();
    } 
    else if (strcmp(token, "myip") == 0) 
    {
        function_handle_cmd_myip();
    } 
    else if (strcmp(token, "myport") == 0) 
    {
        printf("My port: %d\n", port);
    } 
    else if (strcmp(token, "connect") == 0) 
    {
        function_handle_cmd_connect(token, port);
    } 
    else if (strcmp(token, "list") == 0) 
    {
        function_handle_cmd_list();
    } 
    else if (strcmp(token, "terminate") == 0) 
    {
        function_handle_cmd_terminate(token);
    } 
    else if (strcmp(token, "send") == 0) 
    {
        function_handle_cmd_send(token, port);
    } 
    else if (strcmp(token, "exit") == 0) 
    {
        function_handle_cmd_exit();
    } 
    else 
    {
        printf("Unknown command: '%s'. Type 'help' for list of commands.\n", token);
    }
}

// handle remote connection
void *function_handle_client(void *arg) 
{
    int client_sock = *(int *)arg;
    free(arg); // Giải phóng bộ nhớ
    char buffer[BUFFER_SIZE];
    while (1) 
    {
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) 
        {
            printf("Connection closed by peer\n");
            for (int i = 0; i < MAX_CONNECTIONS; i++) 
            {
                if (device_connect[i].sockfd == client_sock && device_connect[i].active) 
                {
                    printf("Disconnected from %s:%d\n", device_connect[i].ip, device_connect[i].port);
                    device_connect[i].active = 0;
                    connection_count--;
                    close(client_sock);
                    break;
                }
            }
            return NULL;
        }
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
    }
    return NULL;
}
/*-------------------------- FUNCTION - END   -------------------------*/