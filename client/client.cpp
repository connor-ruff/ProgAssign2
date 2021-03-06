// Connor Ruff - cruff
// Kelly Buchanan - kbuchana
// Ryan Wigglesworth - rwiggles
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <iostream>
#include <string>
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <unistd.h>


char* PROGRAM_NAME;

std::string get_user_input(){
	std::cout << "> ";
	std::string command;
	getline(std::cin, command);
	return command;
}


void usage(int arg){
	std::cout << PROGRAM_NAME << "[SERVER] [PORT]\n";
	exit(arg);
}

std::string get_arg(std::string command){
	/* seperate the arg from the command */
	return command.substr(command.find_last_of(' ', command.size())+1);
}

int get_socket(){
	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		std::cerr << "Could not create socket\n";
	   	return -1;	
	}	
	return fd;

}

size_t sendToServ(int fd, void * toSend, int len) {

	size_t sent = send(fd, toSend, len, 0);
	if ( sent == -1 ){
		std::cerr << "Error Sending Info To Client: " << strerror(errno) << std::endl;
		std::exit(-1);
	}

	return sent;

}

void * getServMsg(int fd, int len) {

	char buf[BUFSIZ];
	int rec;

	bzero(&buf, sizeof(buf));
	rec = recv(fd, buf, len, 0);

	if (rec == -1 ){
		std::cerr << "Client Failed on recv(): " << strerror(errno) << std::endl;
		std::exit(-1);
	}
	buf[rec] = '\0';

	void * ret = buf;
	return ret;
}

void handle_DN(int fd, std::string command){
/* Client Side implementation of the DN request */
	int valread;
	char buffer[BUFSIZ];
	std::string arg = get_arg(command);
	int code = 1;
	sendToServ(fd, (void *)&code, sizeof(int));  // 1 = code for "download" command
	
	// Send Size of Fucking FIl;ename
	short int nameSiz = arg.length() + 1;
	sendToServ(fd, (void *)&nameSiz, sizeof(short int));

	// Send over the filename
	sendToServ(fd, (void *) arg.c_str(), nameSiz);
	
	// Recieve the size of the file
	int fileSize;
	if (  ( valread = read(fd, (int *)&fileSize, sizeof(int))) == -1){
		std::cerr << "Failure on read(): " << strerror(errno) << std::endl;
		std::exit(-1);
	}

	// Check If Server Sent Error Indicator
	if (fileSize == -1){
		std::cout << "No file found at " << arg << std::endl;
		return;
	}

	//Read in the md5hash
	if (   (valread = read(fd, buffer, 33)) == -1 ) {
		std::cerr << "Failure on read(): " << strerror(errno) << std::endl;
		std::exit(-1);
	}

	buffer[valread] = '\0';
	std::string md5sum = std::string(buffer);

	int totalSent = 0;
	bzero( &buffer, sizeof(buffer));
	// Start Calculating time
	struct timeval b4 ;
	gettimeofday(&b4, NULL);


	// Manipulate recieve maximum based on filesize
	size_t sizey;
	if (fileSize > BUFSIZ){
		sizey = BUFSIZ;
	}
	else 
		sizey = fileSize;


	FILE * wf = fopen(arg.c_str(), "wb"); // where to store file

	while( totalSent < fileSize ){
		valread = recv(fd, buffer, sizey, 0);
		totalSent += valread;
		fwrite(buffer, 1, valread, wf);                            
		bzero( &buffer, sizeof(buffer));
		 if ( totalSent >= fileSize ) {
			break;
		}
	}


	struct timeval after;
    gettimeofday(&after, NULL);
	fclose(wf);
		
	
	float elapsedTime = ( ((after.tv_sec - b4.tv_sec) * 1000000) + (after.tv_usec - b4.tv_usec) ) ;
	float throughPut = (  (fileSize * 8) ) / ( elapsedTime / 1000000) ;
	std::cout << fileSize << " bytes transferred in " << elapsedTime / 1000000 << " seconds: " << throughPut << " bits/sec" << std::endl;

	// calculate the md5sum and print out match status
	char newmd5sum [40] = "md5sum ";
	strcat(newmd5sum, arg.c_str());
	FILE * dfile = popen(newmd5sum, "r");
	char md5sumOutput [50] ;
	fgets(md5sumOutput, 50, dfile);

	char * hash = strtok(md5sumOutput, " ");
	std::cout << "MD5 Hash: " << md5sumOutput ;
	if ( !strcmp(md5sum.c_str(), hash) ){
		std::cout << " (matches)"  << std::endl;
	}
	else {
		std::cout << "\nERROR: Hash's do not match. Download CORRUPTED" << std::endl;
		return;
	}

	pclose(dfile);
}

void handle_UP(int servFD, std::string arg){
	/* This is the client side implementation of the UP command.
	 * basically, should allow the server to get uploaded file */
	std::string filename = get_arg(arg);

	FILE *fd = fopen(filename.c_str(), "rb");
	int check;
	if(!fd){
		std::cout << "File Not Found" << std::endl;
		return;
	}
	// send notice we intend to upload
	int comCode = 2;
	sendToServ(servFD, (void *)&comCode, sizeof(int));
	
	// Send size of filename
	short int fileLen = (short int) (filename.length() + 1);
	sendToServ(servFD, (void *)&fileLen, sizeof(short int));
	// Send filename
	sendToServ(servFD, (void *)filename.c_str(), fileLen ) ;

	// Send file size
	struct stat stat_file;
	if ((stat(filename.c_str(), &stat_file)) == -1){
		std::cerr << "Error on Stat: " << strerror(errno) << std::endl;
	}
	check = stat_file.st_size;
	int sizey = check;
	size_t sent_check = sendToServ(servFD, (void *)&check, sizeof(int));
	if (sent_check == -1)
		std::cerr <<"Sending Info To Client: " << strerror(errno) << std::endl;
	// Get md5Sum for later
	char mdsum[40] = "md5sum ";
	strcat(mdsum, filename.c_str());
	FILE* fsum= popen(mdsum, "r");
	char md5sumOutput[50];
	fgets(md5sumOutput, 50, fsum);
	pclose(fsum);
	char * hash = strtok(md5sumOutput, " ");

	// recieve acknowlegement
	int reccode = 0;
	while (reccode != 1)
		read(servFD, (void *)&reccode, sizeof(0));
	

	char buf[BUFSIZ];
	size_t num;
	do {
		bzero(&buf, BUFSIZ);
		if (check < BUFSIZ) {
			num = check;
		} else {
			num = BUFSIZ;
		}

		num = fread(buf, 1, num, fd);
		if (num < 1)
			break;
		if(!send(servFD, (void *)buf, num, 0))
			break;
		check -= num;
	}
	while (check > 0);
	
	fclose(fd);

	std::cout << "Calculated: " << hash << std::endl;

	// Recieve the throughput
	float throughPut;
	throughPut = *((float *) getServMsg( servFD, sizeof(throughPut)));
	std::cout << sizey << " bytes transferred with throughput of: " ; 
	std::cout << throughPut << " bits/sec" << std::endl;
	int valread;
	// recieve the hash
	char * buffer = (char *) (getServMsg(servFD, BUFSIZ));
	std::cout << "MD5 Hash: " << buffer ;
	if(!strcmp(buffer, hash))
		std::cout << " (matches)" << std::endl;
	else
		std::cout << " - Hashes do not match, CORRUPTED\n";
}


void handle_HEAD(int servFD, std::string command){
	/* Client side implementation of HEAD. 
	 * prints first 10 lines of a file */
	int valread;
	char buffer[BUFSIZ];
	std::string filename = get_arg(command);

	// Signal that we want to do HEAD
	int comcode = 3;
	send(servFD, (void *)&comcode, sizeof(comcode), 0);

	// send the length of the filename
	short int fileLen = (short int) filename.length()+1;
	send(servFD, (void *)&fileLen, sizeof(short int), 0);
	// Send filename
	send(servFD, filename.c_str(), strlen(filename.c_str()) + 1, 0);

	// Recieve the size of the file as a 32 bit int
	// possible endian probs
	int fileSize;
	valread = read(servFD, (int *)&fileSize, sizeof(fileSize));
	if (fileSize == -1){
		std::cout << "No file found at " << filename << std::endl;
		return;
	}
	// Manipulate recieve maximum based on filesize
	size_t sizey;
	if (fileSize > BUFSIZ){
		sizey = BUFSIZ;
	}
	else 
		sizey = fileSize;


	int totalSent = 0;
	while( totalSent < fileSize ){
		valread = recv(servFD, buffer, sizey, 0);
		totalSent += valread;
		fwrite(buffer, 1, valread, stdout); 
		bzero( &buffer, sizeof(buffer));
		 if ( totalSent >= fileSize ) {
			break;
		}
	}
}



void handle_LS(int fd, std::string input){

	
	std::string arg = get_arg(input);

	// Send LS Flag To Server
	int opCode = 5;
	send(fd, (void *)&opCode, sizeof(int), 0);

	char buf[BUFSIZ];
	int bufSiz;
	recv(fd, (void *)&bufSiz, sizeof(int), 0);
	recv(fd, buf, bufSiz, 0);
	std::cout << buf << std::endl;
}

void handle_CD(int fd, std::string input){
	std::string arg = get_arg(input);

	// Send LS Flag To Server
	int opCode = 8;
	send(fd, (void *)&opCode, sizeof(int), 0);

	// Send Size of DirName
	short int dirSiz = arg.length() + 1;
	send(fd, (void *)&dirSiz, sizeof(short int), 0);
	// Send DirName
	send(fd, arg.c_str(), dirSiz, 0);

	// Get Server Response
	int resp;
	recv(fd, (void *)&resp, sizeof(int), 0);
	switch (resp){
		case -2: 
			std::cout << "The directory does not exist on server" << std::endl;
			break;
		case -1:
			std::cout << "Error in Changing Directory" << std::endl;
			break;
		default:
			std::cout << "Changed current directory" << std::endl;
	}
			
}

void handle_MKDIR(int fd, std::string input){

	std::string arg = get_arg(input);

	// SEND MKDIR FLAG
	int opCode = 6;
	send(fd, (void *)&opCode, sizeof(int), 0);

	// Send size of dirName
	short int dirSiz = arg.length()+1;
	send(fd, (void *)&dirSiz, sizeof(short int), 0);
	// Send DirName
	send(fd, arg.c_str(),dirSiz, 0);

	int resp;
	recv(fd, (void *)&resp, sizeof(int), 0);
	switch (resp){
		case -2: 
			std::cout << "The directory already exists on the server" << std::endl;
			break;
		case -1:
			std::cout << "Error in making directory" << std::endl;
			break;
		default:
			std::cout << "The directory was successfully made" << std::endl;
	}

}
void handle_RMDIR(int fd, std::string input){

	std::string arg = get_arg(input);

	// SEND RMDIR FLAG
	int opCode = 7;
	send(fd, (void *)&opCode, sizeof(int), 0);

	// Send size of dirName
	short int dirSiz = arg.length()+1;
	send(fd, (void *)&dirSiz, sizeof(short int), 0);
	// Send DirName
	send(fd, arg.c_str(),dirSiz, 0);

	int resp;
	recv(fd, (void *)&resp, sizeof(int), 0);
	switch (resp){
		case -2: 
			std::cout << "Error in removing directoy" << std::endl;
			return;
		case -1:
			std::cout << "The directory does not exist" << std::endl;
			return;
		default: {
			std::cout << "Are you sure you want to remove " << arg <<"?: (y/n) " << std::endl;
			char in;
			std::cin >> in;
			std::cin.ignore();
			if (in == 'y'){
				int code = 1;
				send(fd, (void *)&code, sizeof(int), 0);
			}
			else {
				int code = -1;
				send(fd, (void *)&code, sizeof(int), 0);	
			}
			break;

		}
	}

	recv(fd, (void *)&resp, sizeof(int), 0);
	if (resp == 1){
		std::cout << "Directory Removed" << std::endl;
	}
	else
		std::cout << "Directory Not Remove" << std::endl;


}

void handle_RM(int fd, std::string input){

	std::string arg = get_arg(input);

	// SEND RM FLAG
	int opCode = 4;
	send(fd, (void *)&opCode, sizeof(int), 0);

	// Send size of dirName
	short int dirSiz = arg.length()+1;
	send(fd, (void *)&dirSiz, sizeof(short int), 0);
	// Send DirName
	send(fd, arg.c_str(),dirSiz, 0);

	int resp;
	recv(fd, (void *)&resp, sizeof(int), 0);
	switch (resp){
		case -1:
			std::cout << "The file does not exist" << std::endl;
			return;
		default: {
			std::cout << "Are you sure you want to remove " << arg <<"?: (y/n) " << std::endl;
			char in;
			std::cin >> in;
			std::cin.ignore();
			if (in == 'y'){
				int code = 1;
				send(fd, (void *)&code, sizeof(int), 0);
			}
			else {
				int code = -1;
				send(fd, (void *)&code, sizeof(int), 0);	
			}
			break;

		}
	}

	recv(fd, (void *)&resp, sizeof(int), 0);
	if (resp == 0){
		std::cout << "File Removed" << std::endl;
	}
	else
		std::cout << "File Not Remove" << std::endl;


}


int main(int argc, char* argv[]){
	struct hostent *hp; // host info
	struct sockaddr_in servaddr; // server address
	socklen_t addrlen = sizeof(servaddr);
	// Handle IO
	PROGRAM_NAME = argv[0];
	if (argc != 3)
		usage(EXIT_FAILURE);
	char* serverName = argv[1];
	int port = atoi(argv[2]);

	// Create Socket	
	int fd = get_socket();
	if (fd < 0){
		std::cout << "Creating file descriptor failed\n";
		exit(EXIT_FAILURE);
	}

	memset((char*)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	// Get host name
	hp = gethostbyname(serverName);
	if(!hp){
		fprintf(stderr, "could not obtain address of %s\n", serverName);
	}
	// put hosts address into server address structure
	memcpy((void *)&servaddr.sin_addr, hp->h_addr_list[0], hp->h_length);


	// connect 
	if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		printf("\nConnection Failed \n");
		return -1; //TODO add back as soon as io works
	}
	

	// Prompt the user for input and handle it
	while(true){

		// Send out message
		std::string user_input = get_user_input();
		std::string command = user_input.substr(0, user_input.find(" "));
		//send(fd, user_input.c_str(), strlen(user_input.c_str()), 0);
		if (command == "DN"){
			handle_DN(fd, user_input);
		} else if (command == "UP"){
			handle_UP(fd, user_input);
		} else if (command == "HEAD"){
			handle_HEAD(fd, user_input);
		} else if (command == "RM"){
			handle_RM(fd, user_input);
		} else if (command == "LS"){
			handle_LS(fd, user_input);
		} else if (command == "RMDIR"){
			handle_RMDIR(fd, user_input);
		} else if (command == "CD"){
			handle_CD(fd, user_input);
		} else if (command == "QUIT"){
			int com = 9;
                        send(fd, (void *)&com, sizeof(int), 0);
                        close(fd);
                        return 0;
		} else if (command == "MKDIR"){
			handle_MKDIR(fd, user_input);
		} else if ( command == ""){
			continue;
		}
		else {
                    std::cout << "Command not recognized." << std::endl;
		}

	}
	close(fd);
        return 0;
	
}
