#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <libgen.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/signal.h>

#define NUMBER_OF_THREADS 10

#pragma region structuri

typedef struct dictionary
{
	char *word;
	int freq;
} dictionary;

typedef struct threadParams
{
	int sockfd, index_id;
} threadParams;

typedef struct file
{
	char *filename;
	pthread_mutex_t fmutex;
	dictionary *word_freq;
} file;

#pragma endregion

#pragma region variabile globale

pthread_t accept_id;
pthread_t search_thread;
int sockfd, portno, clilen;
struct sockaddr_in serv_addr, cli_addr;
pthread_t id[NUMBER_OF_THREADS] = {0};
pthread_mutex_t files_mutex = PTHREAD_MUTEX_INITIALIZER;
file *files_in_system;
int nr_of_files = 0;
int logger_fd;
pthread_mutex_t logger_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_cond = PTHREAD_MUTEX_INITIALIZER;
char *path_for_freq;

#pragma endregion

#pragma region functiile serverului

void list(const char *path)
{
	DIR *dir;
	struct dirent *fis;
	dir = opendir(path);
	while ((fis = readdir(dir)) != NULL)
	{
		if (fis->d_name[0] != '.')
		{
			if (fis->d_type != 4)
			{
				pthread_mutex_lock(&files_mutex);
				files_in_system = realloc(files_in_system, (nr_of_files + 1) * sizeof(file));
				files_in_system[nr_of_files].filename = calloc(256, sizeof(char));
				strcpy(files_in_system[nr_of_files].filename, path);
				strcat(files_in_system[nr_of_files].filename, fis->d_name);
				pthread_mutex_init(&files_in_system[nr_of_files].fmutex, NULL);
				nr_of_files++;
				pthread_mutex_unlock(&files_mutex);
			}
			else
			{
				char newnewpath[1024];
				sprintf(newnewpath, "%s%s/", path, fis->d_name);

				list(newnewpath);
			}
		}
	}
}

int first_null()
{
	for (int i = 0; i < 10; i++)
		if (id[i] == 0)
			return i;
		else
			return -1;
}

void logger(char *tip_operatie, char *next_param)
{
	time_t current_time = time(NULL);
	char *char_time = calloc(256, sizeof(char));
	char_time = ctime(&current_time);
	char *buffer = calloc(512, sizeof(char));
	strcpy(buffer, char_time);
	buffer[strlen(buffer) - 1] = ' ';
	if (next_param != NULL)
		sprintf(buffer + strlen(buffer), "[%s] [%s]\n", tip_operatie, next_param);
	else
		sprintf(buffer + strlen(buffer), "[%s]\n", tip_operatie);
	pthread_mutex_lock(&logger_mutex);
	write(logger_fd, buffer, strlen(buffer));
	pthread_mutex_unlock(&logger_mutex);

	free(buffer);
}

void *thread_freq(void *p);

void update_list()
{
	pthread_mutex_lock(&files_mutex);
	nr_of_files = 0;
	pthread_mutex_unlock(&files_mutex);
	list("./Resources/");
	for (int i = 0; i < nr_of_files; i++)
	{
		pthread_mutex_lock(&files_mutex);
		strcpy(files_in_system[i].filename, files_in_system[i].filename + 12);
		pthread_mutex_unlock(&files_mutex);
		pthread_mutex_init(&files_in_system[i].fmutex, NULL);

		while (path_for_freq != NULL)
			;

		path_for_freq = strdup(files_in_system[i].filename);

		pthread_mutex_lock(&mutex_cond);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex_cond);
	}
}

pthread_t initialise()
{
	logger_fd = open("log.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);

	pthread_t id;
	pthread_create(&id, NULL, thread_freq, NULL);
	sleep(1);
	update_list();

	return id;
}

int where_in_struct(char *file_nm)
{
	for (int i = 0; i < nr_of_files; i++)
		if (strcmp(files_in_system[i].filename, file_nm) == 0)
			return i;

	return -1;
}

void remove_duplicates(dictionary *dict, int *nr_cuv)
{
	for (int i = 0; i < *nr_cuv - 1; i++)
		for (int j = i + 1; j < *nr_cuv; j++)
			if (strcmp(dict[i].word, dict[j].word) == 0)
			{
				dictionary aux;
				aux = dict[i];
				dict[i] = dict[j];
				dict[j] = aux;
				(*nr_cuv)--;
			}
}

void set_freq_word(char *fis, int size, int nr_cuv, dictionary *dict)
{
	int bytes_read = 0;
	while (bytes_read < size)
	{
		char *word = calloc(256, sizeof(char));
		sscanf(fis, "%s", word);
		fis += strlen(word) + 1;
		bytes_read += strlen(word) + 1;
		for (int i = 0; i < nr_cuv; i++)
			if (strcmp(word, dict[i].word) == 0)
			{
				dict[i].freq++;
				break;
			}
	}
}

void sort_dict(dictionary *dict, int nr_cuv)
{
	for (int i = 0; i < nr_cuv - 1; i++)
		for (int j = i + 1; j < nr_cuv; j++)
			if (dict[i].freq < dict[j].freq)
			{
				dictionary aux;
				aux = dict[i];
				dict[i] = dict[j];
				dict[j] = aux;
			}
}

void copy_freq(dictionary *dict, int index)
{
	files_in_system[index].word_freq = calloc(10, sizeof(dictionary));
	for (int i = 0; i < 10; i++)
	{
		files_in_system[index].word_freq[i].word = strdup(dict[i].word);
		files_in_system[index].word_freq[i].freq = dict[i].freq;
	}
}

void set_freq(int index)
{
	char *path = calloc(strlen("./Resources/") + strlen(files_in_system[index].filename), sizeof(char));
	strcpy(path, "./Resources/");
	strcat(path, files_in_system[index].filename);
	int fd = open(path, O_RDONLY);
	struct stat aux;
	lstat(path, &aux);
	char *fis = mmap(NULL, aux.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	char *cfis = fis;
	int bytes_read = 0, nr_cuv = 0;
	dictionary *dict = calloc(1, sizeof(dictionary));

	while (bytes_read < aux.st_size)
	{
		char *word = calloc(256, sizeof(char));
		sscanf(fis, "%s", word);
		bytes_read += strlen(word) + 1;
		fis = fis + strlen(word) + 1;
		nr_cuv++;
		dict = realloc(dict, nr_cuv * sizeof(dictionary));
		dict[nr_cuv - 1].word = strdup(word);
	}
	remove_duplicates(dict, &nr_cuv);
	fis = cfis;
	set_freq_word(cfis, aux.st_size, nr_cuv, dict);
	sort_dict(dict, nr_cuv);
	copy_freq(dict, index);
	munmap(fis, aux.st_size);
	free(dict);
}

#pragma endregion

#pragma region functii tratare

void treat_list(threadParams local)
{
	update_list();
	// de modificat
	// for pentru creat buffer auxiliar pentru send si pentru calcularea dimensiunii sendului
	char *buffer = malloc(sizeof(char));
	char *firstPacket = malloc(256 * sizeof(char));
	memcpy(firstPacket, "\0\0\0\0 ", 5);
	int dimBuffer = 0;
	for (int i = 0; i < nr_of_files; i++)
	{
		buffer = realloc(buffer, (dimBuffer + strlen(files_in_system[i].filename) + 1) * sizeof(char));
		strcpy(buffer + dimBuffer, files_in_system[i].filename);
		dimBuffer += strlen(files_in_system[i].filename) + 1;
		buffer[dimBuffer] = '\0';
	}
	sprintf(firstPacket + 5, "%d ", dimBuffer);
	int firstPckSend = 5 + strlen(firstPacket + 5);
	char *to_send = calloc(firstPckSend + dimBuffer, sizeof(char));
	memcpy(to_send, firstPacket, firstPckSend);
	memcpy(to_send + firstPckSend, buffer, dimBuffer);

	send(local.sockfd, to_send, firstPckSend + dimBuffer, 0);
	logger("List", NULL);
}

void treat_download(char *buffer, threadParams local)
{
	// citest numarul de octeti a caii fisierului
	int nr_bytes_path;
	sscanf(buffer + 5, "%d", &nr_bytes_path);

	//  aduc in cod calea fisierului
	char *path = calloc(nr_bytes_path + strlen("./Resources/"), sizeof(char));
	char *aux = calloc(nr_bytes_path, sizeof(char));

	sscanf(buffer + 5, "%d %s", &nr_bytes_path, aux);
	char *cpy = strdup(aux);
	strcpy(path, "./Resources/");
	strcat(path, aux);
	free(aux);

	// folosesc lstat sa vad cati octeti are
	struct stat file;
	if (lstat(path, &file) == -1)
	{
		/*send eroare*/
		char a[4] = "\0\0\0\0";
		a[3] = 40;
		send(local.sockfd, a, 4, 0);
		return;
	}
	// dau sendfile
	int fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		/*send eroare*/
		char a[4] = "\0\0\0\0";
		a[3] = 40;
		if (errno == EACCES)
			a[3] = 2;
		if (errno == ENOENT)
			a[3] = 1;

		send(local.sockfd, a, 4, 0);
		return;
	}
	aux = calloc(128, sizeof(char));
	sprintf(aux + 4, " %d", (int)file.st_size);
	send(local.sockfd, aux, 4 + sizeof(aux + 4), 0);
	recv(local.sockfd, aux, 1, 0);
	sendfile(local.sockfd, fd, 0, file.st_size);

	logger("Download", cpy);

	free(path);
	free(aux);
	free(cpy);

	close(fd);
}

void treat_upload(char *request, threadParams local)
{
	int bytes_path;
	sscanf(request + 4, "%d", &bytes_path);
	char *path = calloc(bytes_path, sizeof(char));
	int content_size;
	sscanf(request + 4, "%d %s %d", &bytes_path, path, &content_size);
	char *content = calloc(content_size, sizeof(char));
	char *fis_name = basename(path);
	free(path);
	path = calloc(strlen("./Resources/Downloads/") + strlen(fis_name), sizeof(char));
	strcpy(path, "./Resources/Downloads/");
	strcat(path, fis_name);

	int fis_fd = open(path, O_WRONLY | O_CREAT, 0666);

	char a[4] = "\0\0\0\0";
	if (!fis_fd)
	{
		if (errno == EACCES)
			a[3] = 2;
		a[3] == 40;
	}
	send(local.sockfd, a, 4, 0);
	recv(local.sockfd, content, content_size, 0);
	if (!write(fis_fd, content, content_size))
		if (errno == EFBIG)
			a[3] = 4;
		else
			a[3] = 40;
	send(local.sockfd, a, 4, 0);
	if (a[3] == 0)
	{
		logger("Upload", path);
		update_list();
	}

	free(path);
	free(content);
}

void treat_delete(char *request, threadParams local)
{
	int bytes_path;
	sscanf(request + 4, "%d", &bytes_path);
	char *path = calloc(bytes_path, sizeof(char));
	sscanf(request + 4, " %d %s", &bytes_path, path);

	int fd = open(path, O_RDONLY);
	char a[4] = "\0\0\0\0";
	if (fd < 0)
	{
		a[3] = 40;
		if (errno == EACCES)
			a[3] = 2;
		if (errno == ENOENT)
			a[3] = 1;

		send(local.sockfd, a, 4, 0);
		return;
	}
	close(fd);

	if (a[3] == 0)
	{
		logger("Delete", path);
		update_list();
	}

	remove(path);
	free(path);
	send(local.sockfd, a, 4, 0);
}

void treat_move(char *request, threadParams local)
{
	int fdims;
	char status_ret[4] = "\0\0\0\0";
	sscanf(request + 4, "%d", &fdims);

	int i;
	for (i = 0; i < 10000; i++, fdims /= 10)
		if (fdims == 0)
			break;

	char *aux = calloc(256, sizeof(char));
	sscanf(request + 4, "%d %s", &fdims, aux);
	char *path_source = calloc(fdims + strlen("./Resources/"), sizeof(char));
	strcpy(path_source, "./Resources/");
	strcat(path_source, aux);
	char *copy = strdup(path_source);
	int fdimd;
	sscanf(request + 7 + fdims + i, "%d", &fdimd);
	sscanf(request + 7 + fdims + i, "%d %s", &fdimd, aux);
	char *path_dest = calloc(fdimd + strlen(aux), sizeof(char));
	strcpy(path_dest, "./Resources/");
	strcat(path_dest, aux);

	char *real_source = calloc(256, sizeof(char));
	char *real_dest = calloc(256, sizeof(char));
	realpath(path_source, real_source);
	realpath(path_dest, real_dest);

	if (real_dest == NULL || real_source == NULL)
	{
		if (errno == EACCES)
			status_ret[3] = 2;
		else if (errno == ENOENT)
			status_ret[3] = 1;
		else
			status_ret[3] = 40;
	}
	if (rename(real_source, real_dest) == -1)
	{

		if (errno == EACCES)
			status_ret[3] = 2;
		else
			status_ret[3] = 40;
	}
	send(local.sockfd, status_ret, 4, 0);
	if (status_ret[3] == 0)
	{
		logger("Move", copy);
		update_list();
	}

	free(copy);
	free(path_source);
	free(aux);
	free(path_dest);
	free(real_source);
	free(real_dest);
}

void treat_update(char *request, threadParams local)
{
	char status_ret[4] = "\0\0\0\0";
	int dim_path, index_start, dimensiune;
	char *aux = calloc(256, sizeof(char));
	char *content = calloc(1024, sizeof(char));
	sscanf(request + 4, "%d %s %d %d %s", &dim_path, aux, &index_start, &dimensiune, content);
	char *cpy = strdup(aux);
	content = realloc(content, dimensiune * sizeof(char));
	char *path = calloc(dim_path + strlen("./Resources/"), sizeof(char));
	strcpy(path, "./Resources/");
	strcat(path, aux);
	free(aux);

	int file_fd = open(path, O_RDWR);
	if (file_fd == -1)
	{
		/*send eroare*/
		status_ret[3] = 40;
		if (errno == EACCES)
			status_ret[3] = 2;
		if (errno == ENOENT)
			status_ret[3] = 1;

		goto end;
	}
	int index_final = (int)lseek(file_fd, 0, SEEK_END);
	lseek(file_fd, index_start, SEEK_SET);
	char *tmp_content = calloc(index_final - index_start, sizeof(char));
	read(file_fd, tmp_content, index_final - index_start);
	lseek(file_fd, index_start, SEEK_SET);
	write(file_fd, content, dimensiune);
	lseek(file_fd, index_start + dimensiune, SEEK_SET);
	write(file_fd, tmp_content, index_final - index_start);
	free(tmp_content);
	close(file_fd);
end:
	send(local.sockfd, status_ret, sizeof(unsigned int), 0);
	if (status_ret[3] == 0)
	{
		logger("Update", cpy);
		update_list();
	}
	free(content);
	free(cpy);
	free(path);
}

void treat_search(char *request, threadParams local)
{
	char *buffer = calloc(512, sizeof(char));
	char status_ret[4] = "\0\0\0\0";
	int size = 0;
	int bytes_word;
	sscanf(request + 4, "%d", &bytes_word);
	char *word = calloc(bytes_word, sizeof(char));
	sscanf(request + 4, "%d %s", &bytes_word, word);

	pthread_mutex_lock(&files_mutex);

	for (int i = 0; i < nr_of_files; i++)
		for (int j = 0; j < 10; j++)
			if (strcmp(files_in_system[i].word_freq[j].word, word) == 0)
			{
				strcpy(buffer + size, files_in_system[i].filename);
				size += strlen(files_in_system[i].filename) + 1;
				break;
			}
	pthread_mutex_unlock(&files_mutex);
	buffer = realloc(buffer, size * sizeof(char));
	char *to_send = calloc(512, sizeof(char));
	memcpy(to_send, status_ret, sizeof(unsigned int));
	sprintf(to_send + 4, " %d /", size);
	int aux = strlen(to_send + 4) - 1;
	int i = 0;
	for (i = 0; i < 10; i++)
		if (to_send[i] == '/')
			break;

	memcpy(to_send + i, buffer, size);
	send(local.sockfd, to_send, size + 4 + aux, 0);
	logger("Search", word);
	free(buffer);
	free(word);
	free(to_send);
}

#pragma endregion

#pragma region functii threaduri

void *thread_routine(void *p)
{
	threadParams local = *(threadParams *)p;
	while (1)
	{
		char *buffer = calloc(1024, sizeof(char));
		int message = recv(local.sockfd, buffer, 1024, 0);

		if (message < 0)
			printf("ERROR reading from socket");

		unsigned int icode;
		memcpy(&icode, buffer, sizeof(unsigned int));
		icode = ntohl(icode);

		switch (icode)
		{
		case 0:
			treat_list(local);
			break;
		case 1:
			treat_download(buffer, local);
			break;
		case 2:
			treat_upload(buffer, local);
			break;
		case 4:
			treat_delete(buffer, local);
			break;
		case 8:
			treat_move(buffer, local);
			break;
		case 10:
			treat_update(buffer, local);
			break;
		case 20:
			treat_search(buffer, local);
			break;
		case 40:
			id[local.index_id] = 0;
			shutdown(local.sockfd, 2);
			close(local.sockfd);
			return NULL;
			break;
		default:
			char a[4];
			a[0] = a[1] = a[2] = 0;
			a[3] = 10;
			send(local.sockfd, a, sizeof(unsigned int), 0);
			break;
		}
		free(buffer);
	}

	return NULL;
}

void *accept_thread(void *p)
{
	int opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	// initializez serv_addr cu 0-uri
	bzero((char *)&serv_addr, sizeof(serv_addr));

	portno = 2002;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		printf("ERROR on binding");

	listen(sockfd, 10);

	while (1)
	{
	start_while:
		clilen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		printf("Connection has been tried\n");
		if (newsockfd < 0)
			goto start_while;

		int index = first_null();
		char a[4] = "\0\0\0\0";
		if (index == -1)
		{
			a[3] = 8;
			send(newsockfd, a, sizeof(unsigned int), 0);
			close(newsockfd);
			shutdown(newsockfd, 2);
			goto start_while;
		}

		send(newsockfd, a, sizeof(unsigned int), 0);
		threadParams local;
		local.sockfd = newsockfd;
		local.index_id = index;
		pthread_create(&id[index], NULL, thread_routine, &local);
	}
}

void *thread_freq(void *p)
{
	do
	{
		free(path_for_freq);
		path_for_freq = NULL;
		pthread_mutex_lock(&mutex_cond);
		pthread_cond_wait(&cond, &mutex_cond);

		int index = where_in_struct(path_for_freq);
		if (index > 0)
		{
			pthread_mutex_lock(&files_in_system[index].fmutex);
			set_freq(index);
			pthread_mutex_unlock(&files_in_system[index].fmutex);
		}
		pthread_mutex_unlock(&mutex_cond);
	} while (path_for_freq != NULL);
}

void *thread_exit(void *p)
{
	int sfd = (*(int *)p);

	struct epoll_event ev;

	int epfd = epoll_create1(0);

	ev.data.fd = STDIN_FILENO;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

	ev.data.fd = sfd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);

	while (1)
	{
		struct epoll_event ret_ev;

		epoll_wait(epfd, &ret_ev, 1, -1);

		if ((ret_ev.data.fd == sfd) && ((ret_ev.events & EPOLLIN) != 0))
		{
			break;
		}
		else if ((ret_ev.data.fd == STDIN_FILENO) && ((ret_ev.events & EPOLLIN) != 0))
		{
			char *buffer = calloc(128, sizeof(char));
			fgets(buffer, 128, stdin);
			if (strcmp(buffer, "quit\n") == 0)
			{
				break;
			}
		}
	}

	pthread_cancel(accept_id);

	for (int i = 0; i < NUMBER_OF_THREADS; i++)
	{
		if (id[i] != 0)
			pthread_join(id[i], NULL);
	}

	free(path_for_freq);
	nr_of_files = 0;
	path_for_freq = NULL;

	pthread_mutex_lock(&mutex_cond);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex_cond);
	printf("Server shutdown\n");
	pthread_join(search_thread, NULL);
	close(sfd);
	close(epfd);
}

#pragma endregion

int main()

{
	sigset_t mask;
	int sfd;
	struct signalfd_siginfo fdsi;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	sigprocmask(SIG_BLOCK, &mask, NULL);

	sfd = signalfd(-1, &mask, 0);

	search_thread = initialise();
	pthread_t exit_thread_id;
	pthread_create(&exit_thread_id, NULL, thread_exit, &sfd);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		printf("ERROR opening socket");

	pthread_create(&accept_id, 0, accept_thread, NULL);

	pthread_join(accept_id, NULL);

	pthread_join(exit_thread_id, NULL);

	close(logger_fd);
	close(sockfd);
}