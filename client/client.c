#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

int sockfd, portno, n;
struct sockaddr_in serv_addr;
struct hostent *server;

#pragma region Auxiliary functions

int treat_status(int status)
{
	switch (status)
	{
	case 0:
		printf("Operation has been succesfully executed!\n");
		return 1;
		break;

	case 1:
		printf("File not found\n");
		return -1;
		break;

	case 2:
		printf("Permission denied\n");
		return -1;
		break;

	case 4:
		printf("Out of memory\n");
		return -1;
		break;

	case 8:
		printf("Server busy\n");
		return -1;
		break;

	case 10:
		printf("Unknown operation\n");
		return -1;
		break;

	case 20:
		printf("Bad arguments\n");
		return -1;
		break;

	default:
		printf("Other error\n");
		return -1;
		break;
	}
}

void close_connection()
{
	char a[4];
	a[0] = a[1] = a[2] = 0;
	a[3] = 40;
	send(sockfd, a, sizeof(unsigned int), 0);
	shutdown(sockfd, 2);
	close(sockfd);
	printf("Thank you for using our services!\n");
	exit(1);
}

#pragma endregion

#pragma region Client functions

int print_list()
{
	char *buffer = (char *)calloc(256, sizeof(char));

	char buffet[36] = "\0\0\0\0";
	send(sockfd, buffet, 36, 0);

	n = recv(sockfd, buffer, 255, 0);

	if (n < 0)
		printf("ERROR reading from socket");

	unsigned int status;
	memcpy(&status, buffer, sizeof(unsigned int));
	status = ntohl(status);
	if (!treat_status(status))
		return -1;
	int nr_octeti;
	sscanf(buffer + 5, "%d", &nr_octeti);
	buffer += 5;
	for (int i = 0; i < 10; buffer++)
		if (buffer[i] == ' ')
			break;
	buffer++;
	if (n < 0)
		printf("ERROR reading from socket");

	for (int current_read = 0; current_read < nr_octeti; current_read++)
	{
		printf("%s\n", buffer);
		current_read += strlen(buffer);
		buffer += strlen(buffer) + 1;
	}
	return 1;
}

int download()
{
	char *buffer = calloc(256, sizeof(char));
	buffer[3] = '\1';
	char *path = calloc(256, sizeof(char));
	printf("Insert path: ");
	scanf("%s", path);
	getc(stdin);
	char *cpath = strdup(path);
	sprintf(buffer + 4, " %d %s", (int)strlen(path), path);
	send(sockfd, buffer, strlen(buffer + 4) + 4, 0);

	buffer = calloc(256, sizeof(char));
	recv(sockfd, buffer, 256, 0);
	unsigned int status;
	memcpy(&status, buffer, sizeof(unsigned int));
	status = ntohl(status);
	if (!treat_status(status))
		return -1;

	int bytes_to_write;
	sscanf(buffer + 5, "%d", &bytes_to_write);
	char *content = calloc(bytes_to_write, sizeof(char));
	send(sockfd, "y", 1, 0);
	recv(sockfd, content, bytes_to_write, 0);

	free(path);
	path = strdup("./Resources/Downloads/");
	char *fis_name = strdup(basename(cpath));
	path = realloc(path, strlen(path) + strlen(fis_name));
	strcat(path, fis_name);
	int fis_fd = open(path, O_CREAT | O_WRONLY, 0666);
	write(fis_fd, content, bytes_to_write);
	free(content);
	free(path);
	free(fis_name);
	return 1;
}

int upload()
{
	// cod op
	char status[4] = "\0\0\0\2";
	printf("Insert path: ");
	char *path = calloc(256, sizeof(char));
	scanf("%s", path);
	getc(stdin);
	path = realloc(path, strlen(path));
	char *newpath = strdup(path);
	path = strdup("./Resources/");
	path = realloc(path, strlen(path) + strlen(newpath));
	strcat(path, newpath);
	struct stat file;
	if (lstat(path, &file) == -1)
	{
		if (errno == EACCES)
			treat_status(2);
		else if (errno == ENOENT)
			treat_status(1);
		else
			treat_status(40);
		return -1;
	}

	char *to_send = calloc(256, sizeof(char));
	memcpy(to_send, &status, sizeof(unsigned int));
	sprintf(to_send + 4, " %d %s %d", (int)strlen(path), path, (int)file.st_size);
	int send_fd = open(path, O_RDONLY);
	send(sockfd, to_send, strlen(to_send + 4) + 4, 0);
	char twh[4];
	recv(sockfd, twh, 4, 0);
	int rec_status;
	memcpy(&rec_status, twh, sizeof(unsigned int));
	if (ntohl(rec_status) != 0)
		return -1;
	off_t offset = 0;
	sendfile(sockfd, send_fd, &offset, file.st_size);
	recv(sockfd, twh, 4, 0);
	memcpy(&rec_status, twh, sizeof(unsigned int));
	if (!treat_status(ntohl(rec_status)))
		return -1;

	return 1;
}

int delete()
{
	char status[4] = "\0\0\0\4";
	char *path = strdup("./Resources/");
	char *input_path = calloc(256, sizeof(char));
	printf("Input path: ");
	scanf("%s", input_path);
	getc(stdin);
	path = realloc(path, strlen(path) + strlen(input_path));
	strcat(path, input_path);
	free(input_path);
	char *to_send = calloc(1024, sizeof(char));
	memcpy(to_send, status, sizeof(unsigned int));
	sprintf(to_send + 4, " %d %s", (int)strlen(path), path);
	int dim = 4 + strlen(to_send + 4);
	to_send = realloc(to_send, dim * sizeof(char));
	send(sockfd, to_send, dim, 0);
	int istatus;
	recv(sockfd, &istatus, 4, 0);
	if (!treat_status(ntohl(istatus)))
		return -1;
	free(to_send);
	free(path);
	return 1;
}

int move()
{
	char status[4] = "\0\0\0\0";
	status[3] = 8;
	char *path_source = calloc(256, sizeof(char));
	char *path_dest = calloc(256, sizeof(char));
	int fdims, fdimd;

	printf("Input file path: ");
	scanf("%s", path_source);
	getc(stdin);

	printf("Input destination path: ");
	scanf("%s", path_dest);
	getc(stdin);

	fdims = strlen(path_source);
	fdimd = strlen(path_dest);

	char *buffer = calloc(516, sizeof(char));
	memcpy(buffer, status, sizeof(unsigned));
	sprintf(buffer + 4, " %d %s %d %s ", fdims, path_source, fdimd, path_dest);
	int fdim_total = strlen(buffer + 4) + 4;
	char *auxx = buffer + 4;
	auxx[fdim_total - 5] = '\0';
	char *aux = strstr(buffer + 4, path_source);
	aux[fdims] = '\0';
	send(sockfd, buffer, fdim_total, 0);
	int icode;
	recv(sockfd, &icode, sizeof(unsigned int), 0);
	return treat_status(ntohl(icode));
}

int update()
{
	char status[4] = "\0\0\0\0";
	status[3] = 10;

	char *path = calloc(256, sizeof(char));
	printf("Input path: ");
	scanf("%s", path);
	getc(stdin);
	int dim_path = strlen(path);
	path = realloc(path, dim_path * sizeof(char));

	int index_start;
	printf("Input index start: ");
	scanf("%d", &index_start);
	getc(stdin);

	char *content = calloc(1024, sizeof(char));
	printf("Input content: ");
	scanf("%s", content);
	getc(stdin);
	int dim_content = strlen(content);
	content = realloc(content, dim_content * sizeof(char));

	char *to_send = calloc(2048, sizeof(char));
	memcpy(to_send, status, sizeof(unsigned int));
	sprintf(to_send + 4, " %d %s %d %d %s", dim_path, path, index_start, dim_content, content);
	send(sockfd, to_send, 4 + strlen(to_send + 4), 0);
	int icode;
	recv(sockfd, &icode, sizeof(unsigned int), 0);
	return treat_status(ntohl(icode));
}

int search()
{
	char status[4] = "\0\0\0\0";
	status[3] = 20;
	printf("Input word: ");
	char *word = calloc(128, sizeof(char));
	scanf("%s", word);
	getc(stdin);
	char *buffer = calloc(256, sizeof(char));
	memcpy(buffer, status, sizeof(unsigned int));
	sprintf(buffer + 4, " %d %s", (int)strlen(word), word);

	send(sockfd, buffer, 4 + strlen(buffer + 4), 0);

	recv(sockfd, buffer, 256, 0);
	int icode;
	memcpy(&icode, buffer, sizeof(unsigned int));
	if (treat_status(ntohl(icode)) == -1)
		return -1;
	int size;
	sscanf(buffer + 4, "%d", &size);
	char *aux;
	int i;
	for (i = 6; i < 15; i++)
		if (buffer[i] == ' ')
			break;
	aux = buffer + i + 1;
	for (int j = 0; j < size; j++)
		if (aux[j] == '\0')
			aux[j] = '\n';
	printf("%s", aux);
}

#pragma endregion

int main()
{

	portno = 2002;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		printf("ERROR opening socket");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("Connection failed");
		exit(-1);
	}
	char *aux = malloc(4 * sizeof(char));
	int n = recv(sockfd, aux, 4, 0);

	if (n != 0)
	{
		unsigned int status;
		memcpy(&status, aux, sizeof(unsigned int));
		status = ntohl(status);
		if (!treat_status(status))
			exit(-1);
	}
	else
	{
		perror("Server error");
		exit(-1);
	}

	int command;
	int interface_fd = open("interface.txt", O_RDONLY);
	char *buffer = calloc(512, sizeof(char));

	if (read(interface_fd, buffer, 512) < 0)
		exit(-1);
	close(interface_fd);

	while (1)
	{
		printf("%s\n", buffer);
		scanf("%d", &command);
		getc(stdin);
		switch (command)
		{
		case 0:
			print_list();
			break;
		case 1:
			download();
			break;
		case 2:
			upload();
			break;
		case 4:
			delete ();
			break;
		case 8:
			move();
			break;
		case 10:
			update();
			break;
		case 20:
			search();
			break;
		case 40:
			close_connection();
			break;

		default:
			break;
		}
		printf("\nDo you wish to continue? [y/n]:\n");
		char irellevant;
		scanf("%c", &irellevant);
		getc(stdin);
		if (irellevant == 'n')
			close_connection();
	}
}