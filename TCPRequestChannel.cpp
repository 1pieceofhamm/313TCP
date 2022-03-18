#include "common.h"
#include "TCPRequestChannel.h"
using namespace std;

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR FOR CLASS   R e q u e s t C h a n n e l  */
/*--------------------------------------------------------------------------*/

TCPRequestChannel::TCPRequestChannel (const string hostname, const string port_no){
	if(hostname == ""){
		struct addrinfo data, *server;

		struct sockaddr_storage other_addr;
		socklen_t size;
		char s[INET6_ADDRSTRLEN];
		int rv;

		memset(&data, 0, sizeof(data));
		data.ai_family = AF_UNSPEC; //specifies address family
		data.ai_socktype = SOCK_STREAM;
		data.ai_flags = AI_PASSIVE; //allows socket to bind and accept connections

		if((rv = getaddrinfo(NULL,port_no.c_str(), &data, &server)) != 0){ //gets data and server info
			exit(-1);
		}
		if((sockfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol)) == -1){ //makes socket
			exit(-1);
		}
		if(bind(sockfd, server->ai_addr, server->ai_addrlen) == -1){
			close(sockfd);
			exit(-1);
		}
		freeaddrinfo(server); //free structures
		if(listen(sockfd,20) == -1){
			exit(-1);
		}
		cout<<"Server is ready"<<endl;
		cout<<"portL "<<port_no<<endl;


	}
	else{
		struct addrinfo data, *result;
		memset(&data, 0, sizeof(data));
		data.ai_family = AF_UNSPEC; //specifies address family
		data.ai_socktype = SOCK_STREAM;
		int status;

		if((status = getaddrinfo(hostname.c_str(),port_no.c_str(), &data, &result)) != 0){ //gets data and server info
			exit(-1);
		}
		sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol); //makes socket
		if(sockfd<0){
			exit(-1);
		}
		if(connect(sockfd,result->ai_addr, result->ai_addrlen)<0){
			exit(-1);
		}
		freeaddrinfo(result); //free structures
	}
}
TCPRequestChannel::TCPRequestChannel (int fd){
	sockfd = fd;
}
TCPRequestChannel::~TCPRequestChannel(){ 
	close(sockfd);
}

int TCPRequestChannel::cread(void* msgbuf, int bufcapacity){
	//return read(msgbuf, bufcapacity); 
	return recv(sockfd, msgbuf, bufcapacity, 0);
}

int TCPRequestChannel::cwrite(void* msgbuf, int len){
	//return write(msgbuf, len);
	return send (sockfd, msgbuf, len, 0);
}

int TCPRequestChannel::getfd(){
    return sockfd;
}