#include "so_stdio.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 	/*open*/
#include <sys/stat.h>	/*open*/
#include <fcntl.h>		/*open*/
#include <unistd.h>		/*close, read, write*/
#include <sys/wait.h>   /*wait*/

#define MAX_BUFFER_SIZE 4096
/*structura so_file, care contine indicatori pentru pozitiile
curente in bufferele de scriere/ citire, un indicator pentru EOF,
un indicator pentru eroare si bufferele de scriere/ citire*/
struct _so_file {
	int fd;
	int readStart, readEnd;
	int writeStart, writeEnd;
	int endOfFile;
	int error;
	char lastOperation; /*0 = undefined, 1 = read, 2 = write*/
	char readBuffer[MAX_BUFFER_SIZE];
	char writeBuffer[MAX_BUFFER_SIZE];
};
/*intoarce file descriptorul asociat stream-ului curent*/
int so_fileno(SO_FILE *stream) 
{
	if (stream != NULL) 
		return stream->fd;
	return -1;
}
/*folosesc apelurile ale functiei open pentru a deschide fisierul
in modul specificat de mode; aloc structura de tipul SO_FILE
pe care o eliberez in caz de eroare*/
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE* soFile;
	int fd;

	soFile = (SO_FILE*)malloc(sizeof(SO_FILE));
	soFile->endOfFile = 0;
	soFile->error = 0;
	soFile->lastOperation = 0;
	soFile->readStart = 0;
	soFile->readEnd = 0;
	soFile->writeStart = 0;
	soFile->writeEnd = 0;
	if (!soFile) 
		return NULL;
	if (strcmp(mode, "r") == 0) {
		fd = open(pathname, O_RDONLY);
		if (fd == -1) {
			free(soFile);
			return NULL;
		}
		soFile->fd = fd;
		return soFile;
	}
	if (strcmp(mode, "r+") == 0) {
		fd = open(pathname, O_RDWR);
		if (fd == -1) {
			free(soFile);
			return NULL;
		}
		soFile->fd = fd;
		return soFile;
	}
	if (strcmp(mode, "w") == 0) {
		fd = open(pathname, O_WRONLY|O_CREAT|O_TRUNC,
		 S_IRWXU|S_IRWXG|S_IRWXO);
		if (fd == -1) {
			free(soFile);
			return NULL;
		}
		soFile->fd = fd;
		return soFile;
	}
	if (strcmp(mode, "w+") == 0) {
		fd = open(pathname, O_RDWR|O_CREAT|O_TRUNC,
		 S_IRWXU|S_IRWXG|S_IRWXO);
		if (fd == -1) {
			free(soFile);
			return NULL;
		}
		soFile->fd = fd;
		return soFile;
	}
	if (strcmp(mode, "a") == 0) {
		fd = open(pathname, O_APPEND|O_WRONLY|O_CREAT,
		 S_IRWXU|S_IRWXG|S_IRWXO);
		if (fd == -1) {
			free(soFile);
			return NULL;
		}
		soFile->fd = fd;
		return soFile;
	}
	if (strcmp(mode, "a+") == 0) {
		fd = open(pathname, O_APPEND|O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
		if (fd == -1) {
			free(soFile);
			return NULL;
		}
		soFile->fd = fd;
		return soFile;
	}
	free(soFile);
	return NULL;
}
/*functia care inchide un file descriptor asociat unui stream;
se incearca fflush pe stream-ul curent, apoi se inchide fd si se
elibereaza memoria ocupata de stream; se verifica cazurile de eroare*/
int so_fclose(SO_FILE *stream)
{
	int rez;

	if (stream == NULL) 
		return SO_EOF;
	rez = so_fflush(stream);
	if (rez == -1) {
		stream->error = 1;
		close(stream->fd);
		free(stream);
		return SO_EOF;
	}
	rez = close(stream->fd);
	free(stream);
	if (rez == SO_EOF) 
		return SO_EOF;
	return 0;
}
/*functia care citeste un caracter din stream si il intoarce in buffer;
daca exista caractere necitite in buffer, se citeste urmatorul caracter,
altfel se citeste maxim dimensiunea MAX_BUFFER_SIZE din stream si se
memoreaza in bufferul de citire; se verifica pentru eroare sau EOF
si se seteaza flagurile corespunzator*/
int so_fgetc(SO_FILE *stream)
{
	int rez;

	if (stream == NULL) 
		return SO_EOF;
	stream->lastOperation = 1;
	if ((stream->readStart == stream->readEnd) || 
		(stream->readStart == MAX_BUFFER_SIZE)) {
		rez = read(stream->fd, stream->readBuffer, MAX_BUFFER_SIZE);
		if (rez == 0) {
			stream->endOfFile = 1;
			return SO_EOF;
		}
		if (rez == -1) {
			stream->error = 1;
			return SO_EOF;
		}
		stream->readEnd = rez;
		stream->readStart = 0;
		char aux = stream->readBuffer[stream->readStart];
		stream->readStart++;
		return ((int)(aux));
	}
	else {
		char aux = stream->readBuffer[stream->readStart];
		stream->readStart++;
		return ((int)(aux));
	}
	
}
/*functia care scrie un caracter in stream; daca bufferul de scriere
asociat este plin, mai intai se incearca sa se execute fflush pe stream;
indiferent, ulterior se adauga la sfarsitul bufferului de scriere
urmatorul caracter; se verifica pentru eroare/ se seteaza flag-urile*/
int so_fputc(int c, SO_FILE *stream)
{
	int rez;

	if (stream == NULL) 
		return SO_EOF;
	stream->lastOperation = 2;
	if (stream->writeEnd == MAX_BUFFER_SIZE) {
		rez = write(stream->fd, stream->writeBuffer, stream->writeEnd);
		if (rez <= 0) {
			stream->error = 1;
			return SO_EOF;
		}
		memcpy(stream->writeBuffer,
		 stream->writeBuffer + rez, stream->writeEnd - rez);
		stream->writeEnd = stream->writeEnd - rez;
	}
	stream->writeBuffer[stream->writeEnd] = (char)c;
	stream->writeEnd++;
	return c;
}
/*functia care citeste un numar dat de elemente de o anumita dimensiune
din stream; foloseste apeluri ale functiei so_fgetc in acest sens;
intoarce numarul de elemente citite la intalnirea unei erori in stream,
daca s-a ajuns la sfarsitul fisierului(EOF) sau daca s-au citit toate
elementele specificate*/
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	char rez;
	int readElements = 0;
	int i, j;

	if (stream == NULL) 
		return SO_EOF;
	stream->lastOperation = 1;
	for (i = 0; i < nmemb; i++) {
		for (j = 0; j < size; j++) {
			rez = (char)(so_fgetc(stream));
			if (rez == SO_EOF && stream->endOfFile) 
				return readElements;
			if (stream->error) 
				return readElements;
			memcpy(ptr + size * readElements + j, &rez, 1);
		}
		readElements++;
	}
	return readElements;
}
/*functia care scrie in stream un numar dat de elemente de o anumita
dimensiune; foloseste apeluri succesive ale functiei so_fputc si
intoarce numarul de elemente scrie in stream*/
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	char aux;
	int rez;
	int writtenElements = 0;
	int i, j;

	if (stream == NULL) 
		return SO_EOF;
	stream->lastOperation = 2;
	for (i = 0; i < nmemb; i++) {
		for (j = 0; j < size; j++) {
			aux = *((char*)(ptr + size * writtenElements + j));
			rez = so_fputc((int)aux, stream);
		}
		writtenElements++;
	}
	return writtenElements;
}
/*functia care se asigura ca toate elementele din bufferul de scriere
asociat stream-ului au fost scrise in fisier; seteaza flag-uri, verifica
pentru erori*/
int so_fflush(SO_FILE *stream)
{
	int rez;

	if (stream == NULL) 
		return SO_EOF;
	stream->lastOperation = 0;
	while (stream->writeEnd != 0) {
		rez = write(stream->fd, stream->writeBuffer, stream->writeEnd);
		if (rez < 0) {
			stream->error = 1;
			return SO_EOF;
		}
		memcpy(stream->writeBuffer,
		 stream->writeBuffer + rez, stream->writeEnd - rez);
		stream->writeEnd = stream->writeEnd - rez;
	}
	return 0;
}
/*functia care face seek pentru fd asociat unui stream;
in functie de flag-uri, operatiile efectuate sunt diferite,
conform comportamentului specificat in enunt; se seteaza noile
flag-uri si se verifica pentru erori*/
int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int rez;

	if (stream == NULL) 
		return SO_EOF;
	if (stream->lastOperation == 1) {
		stream->readEnd = 0;
		stream->readStart = 0;
		stream->writeEnd = 0;
	}
	if (stream->lastOperation == 2) {
		if (so_fflush(stream) == SO_EOF) {
			stream->error = 1;
			return SO_EOF;
		}
	}
	stream->lastOperation = 0;
	stream->endOfFile = 0;
	rez = lseek(stream->fd, offset, whence);
	if (rez == SO_EOF) {
		stream->error = 1;
		return SO_EOF;
	}
	return 0;
}
/*am adaugat seek-uri suplimentare pentru a trata cazurile cand din
fisier au fost citite mai multe date (in blocuri, ca urmare a apelurilor
de so_fgetc) decat cate au fost parcurse din buffer*/
long so_ftell(SO_FILE *stream)
{
	long position;
	int rez;

	if (stream == NULL) 
		return SO_EOF;
	if (stream->lastOperation == 2) {
		rez = so_fflush(stream);
		if (rez == SO_EOF) {
			stream->error = 1;
			return SO_EOF;
		}
	}
	stream->lastOperation = 0;
	rez = so_fseek(stream, -(MAX_BUFFER_SIZE - stream->readStart % 
		MAX_BUFFER_SIZE) % MAX_BUFFER_SIZE, SEEK_CUR);
	if (rez == SO_EOF) {
		stream->error = 1;
		return SO_EOF;
	}
	position = lseek(stream->fd, 0, SEEK_CUR);
	if (position == SO_EOF) {
		stream->error = 1;
		return SO_EOF;
	}
	rez = so_fseek(stream, (MAX_BUFFER_SIZE - stream->readStart % 
		MAX_BUFFER_SIZE) % MAX_BUFFER_SIZE, SEEK_CUR);
	if (rez == SO_EOF) {
		stream->error = 1;
		return SO_EOF;
	}
	return position;
}
/*intoarce indicatorul de EOF asociat stream-ului*/
int so_feof(SO_FILE *stream)
{
	if (stream == NULL) 
		return SO_EOF;
	stream->lastOperation = 0;
	return stream->endOfFile;
}
/*intoarce indicatorul de eroare asociat stream-ului*/
int so_ferror(SO_FILE *stream)
{
	if (stream == NULL) 
		return SO_EOF;
	stream->lastOperation = 0;
	return stream->error;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	pid_t pid, wait_ret;
	int status;
	int fd, rez;
	SO_FILE* soFile;
	int myPipe[2];

	rez = pipe(myPipe);
	if (rez != 0) 
		return NULL;
	pid = fork();
	switch (pid) {
	case -1:
		close(myPipe[0]);
		close(myPipe[1]);
		return NULL;
		break;
	case 0:
		if (type == "r") {
			dup2(myPipe[1], STDOUT_FILENO);
			close(myPipe[0]);
			close(myPipe[1]);
		}
		if (type == "w") {
			dup2(myPipe[0], STDIN_FILENO);
			close(myPipe[0]);
			close(myPipe[1]);
		}
		execl("sh", "-c" ,command, NULL);
		exit(EXIT_FAILURE);
		break;
	default:
		/*soFile = (SO_FILE*)malloc(sizeof(SO_FILE));
		if (!soFile) return NULL;
		soFile->endOfFile = 0;
		soFile->error = 0;
		soFile->lastOperation = 0;
		soFile->readStart = 0;
		soFile->readEnd = 0;
		soFile->writeStart = 0;
		soFile->writeEnd = 0;
		if (type == "r") {
			//dup2(fd, STDIN_FILENO);
			soFile->fd = myPipe[0];
			//close(myPipe[0]);
			//close(myPipe[1]);
		}
		if (type == "w") {
			//dup2(fd, STDOUT_FILENO);
			soFile->fd = myPipe[1];
			//close(myPipe[0]);
			//close(myPipe[1]);
		}
		close(myPipe[0]);
		close(myPipe[1]);
		return soFile;
		wait(NULL);*/
		break;
	}
	return NULL;
}
/*similar so_fclose*/
int so_pclose(SO_FILE *stream)
{
	int rez;

	if (stream == NULL) 
		return SO_EOF;
	rez = so_fflush(stream);
	if (rez == -1) {
		stream->error = 1;
		close(stream->fd);
		free(stream);
		return SO_EOF;
	}
	rez = close(stream->fd);
	free(stream);
	if (rez == SO_EOF) 
		return SO_EOF;
	wait(NULL);
	return 0;
}
