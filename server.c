#include <stdio.h>
#include <string.h>
#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"
#include "dg_hdr.h"
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include "unprtt.h"
#include "unprtt_plus.h"
#include <setjmp.h>

#define MAX_INTF 16

struct server_info{
	int sockfd;
	struct sockaddr* IP_addr;
	struct sockaddr* netmask;
	struct sockaddr* subnet_addr;
};

static struct rtt_info rttinfo;
static int rttinit = 0;
static struct msghdr msgsend, msgrecv;
static struct dg_hdr sendhdr, recvhdr;
static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;
int client_window_size = 5;

void sig_alrm(int signo)
{
	printf("\n");
	printf("******in SIGALRM signal handler****\n");
	printf("\n");
	siglongjmp( jmpbuf, 1);
}

void read_file( FILE *fp, struct buf_ele file_buf[], int file_size, int* return_file_buf_size){
	
	int file_buf_size = file_size/PAYLOAD_SIZE + 2;	//use indexing from 1 in file_buf
	int file_data_read_size = PAYLOAD_SIZE;
	int i = 1;
	
	for (i = 1; i < file_buf_size; ++i){
		file_buf[i].seq = i;
		file_buf[i].ts = 0;
		file_buf[i].ack = 0;
		file_buf[i].drop = 0;
		file_buf[i].sent = 0;
		file_buf[i].data_size = 0;
	}

	i = 1;
	while(file_data_read_size == PAYLOAD_SIZE){
		file_data_read_size = fread( (void*)file_buf[i].data, sizeof(char), PAYLOAD_SIZE , fp);
		file_buf[i].data_size = file_data_read_size;
		i++;
	}
	file_buf[i-1].data[file_data_read_size] = '\0';	//null terminate the last data pack
	
	*return_file_buf_size = file_buf_size;
	return;
}

void send_file( struct buf_ele file_buf[], int file_buf_size, int sockfd, struct sockaddr* client_addr ){

	int complete_file_sent = 0, complete_file_ackd = 0;
	int empty_window_size = client_window_size;
	
	int ack_pt = 0, sent_pt = 0;
	int send_now = 0;
	int itr = 0, idx = 1, i = 1;
	int sent_bytes;

	struct iovec iovsend[2], iovrecv[1];


	while( recvhdr.seq < file_buf_size ){
		
		printf("empty window size is %d\n", empty_window_size);

		send_now = empty_window_size - (sent_pt - ack_pt);
		
		if(send_now == 0){
		//	timer
		}
			

		itr = 0;
		while( itr < send_now ){
			//find the index from which to send pack
			i = 1;
			while( file_buf[i].sent == 1 )
				i++;
			
			idx = i; 
			
			//make packet
			sendhdr.seq	=	file_buf[idx].seq;
			sendhdr.ts 	=	rtt_ts(&rttinfo);

			//send packet
			iovsend[0].iov_base = (void*) &sendhdr;
			iovsend[0].iov_len = sizeof(struct dg_hdr);
			iovsend[1].iov_base = (void*) file_buf[idx].data;
			if(file_buf[idx].data_size == PAYLOAD_SIZE)
				iovsend[1].iov_len = file_buf[idx].data_size;
			else
				iovsend[1].iov_len = file_buf[idx].data_size+1;
			
			msgsend.msg_iov = iovsend;
			msgsend.msg_iovlen = 2;
			//sending
			sent_bytes = sendmsg(sockfd, &msgsend, 0);
		//	printf("Sent bytes %d are \n%s\n", idx, iovsend[1].iov_base);
			
			file_buf[idx].sent = 1;	//mark sent
			sent_pt = idx;
			//on to the next one
			itr++;

		}

		//receiving ack
		msgrecv.msg_name = NULL;
		msgrecv.msg_namelen = 0;
		iovrecv[0].iov_base = (void*) &recvhdr;
		iovrecv[0].iov_len = sizeof(struct dg_hdr);
		msgrecv.msg_iov = iovrecv;
		msgrecv.msg_iovlen = 1;
		//receiving
		recvmsg(sockfd, &msgrecv, 0);

		printf("ACK rcvd is %d\n", recvhdr.seq);
		//recording ack
		idx = recvhdr.seq - 1;
		file_buf[idx].ack = 1;
		ack_pt = idx;
		//reading empty window size
		empty_window_size = recvhdr.window_empty;

		printf("Empty window size rcvd from client is %d\n", empty_window_size);
	}

	printf("\n----Recieved ACK for all packets----\n");
	return;
}

int main(int argc, char* argv[]){
	FILE * fp;
	char read_file_line[128];
	size_t len = 128;
	
	const char *temp_ptr= NULL;
	int server_port = 0;
	int idx=0;
	int i = 0, ret = 0, no_of_interface = 0, on =1;
	struct ifi_info* ifi;
	struct ifi_info* ifihead;
	u_char *ptr;
	struct sockaddr *sa;
	struct sockaddr_in *server_addr_in, *network_mask_in, *subnet_addr_in;
	int sockfd;
	struct sockaddr *subnet_addr; 

	char buf_str[64];
	void *buf = NULL;
	int buf_len = 0;
	int recvfrom_len = -1;
	struct sockaddr *recvfrom_addr = NULL;

	struct server_info server_info_list[MAX_INTF];

	fd_set rset, rset_copy;
	int maxfdp1, maxfd = -1;

	pid_t childpid;
	int child_sockfd, child_port, child_server_addr_len = -1;
	char child_port_str[16];
	struct sockaddr *child_server_addr;
	struct sockaddr_in *child_server_addr_in;

	char requested_file_name[32];
	int requested_file_name_len;

	char file_data[512], file_size_str[15];
	int file_data_size = 512, file_size;

	fp = fopen(argv[1], "r");	
	
	if (fp == NULL){
		printf("server.in cannot be opened\nEnter file with port and max sliding window size in the args\n");
		exit(EXIT_FAILURE);
	}

	while (fgets(read_file_line, len, fp) != NULL) {
		if(i==0)
			server_port = atoi(read_file_line);
		if(i==1)
			client_window_size = atoi(read_file_line);
		i++;
	}
	printf("Well known server port is : %d\n", server_port);
	printf("Max sending window sliding size is : %d\n", client_window_size);

	i = 0; 	//index for server_info array
	ifihead = get_ifi_info_plus(AF_INET, 1);

	for( ifi = ifihead; ifi != NULL; ifi = ifi->ifi_next){

/*print the info from ifi_info struct*/
		printf("%s: ", ifi->ifi_name);
		if (ifi->ifi_index != 0)
		printf("(%d) ", ifi->ifi_index);
		printf("<");
		if (ifi->ifi_flags & IFF_UP)            printf("UP ");
		if (ifi->ifi_flags & IFF_BROADCAST)     printf("BCAST ");
		if (ifi->ifi_flags & IFF_MULTICAST)     printf("MCAST ");
		if (ifi->ifi_flags & IFF_LOOPBACK)      printf("LOOP ");
		if (ifi->ifi_flags & IFF_POINTOPOINT)   printf("P2P ");
		printf(">\n");
		if ( (i = ifi->ifi_hlen) > 0) {
			ptr = ifi->ifi_haddr;
			do {
			printf("%s%x", (i == ifi->ifi_hlen) ? "  " : ":", *ptr++);
        		} while (--i > 0);
             		printf("\n");
         	}
		if (ifi->ifi_mtu != 0)
			printf("  MTU: %d\n", ifi->ifi_mtu);
		if ( (sa = ifi->ifi_addr) != NULL)
			printf("  IP addr: %s\n", Sock_ntop_host (sa, sizeof (*sa)));

		((struct sockaddr_in*)sa)->sin_port = 60810;
		server_info_list[idx].IP_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
		server_info_list[idx].IP_addr = sa;

		printf("port is %d\n", ((struct sockaddr_in*)server_info_list[idx].IP_addr)->sin_port);
		/*creating scoket and binding every returned address to a socket*/
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if(sockfd == -1){
			printf("could not create socket for %s\n", Sock_ntop_host(sa, sizeof (*sa)) );
		}
		
		server_info_list[idx].sockfd = sockfd;

		if( (ret = bind( server_info_list[idx].sockfd, server_info_list[idx].IP_addr, sizeof(struct sockaddr)) ) == -1 ){
			printf("could not bind socket to server IP address for %s\n",  Sock_ntop_host(sa, sizeof (*sa)) );
		}
		on = 1;
		if( ( ret = setsockopt(server_info_list[idx].sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on) ) ) < 0 ){
                        printf("setsockopt SO_REUSEADDR failed\n");
		}

		if ( (sa = ifi->ifi_brdaddr) != NULL)
			printf("  broadcast addr: %s\n",
		Sock_ntop_host (sa, sizeof(*sa)));
		
		if ( (sa = ifi->ifi_dstaddr) != NULL)
			printf("  destination addr: %s\n",
		Sock_ntop_host(sa, sizeof(*sa)));


		if((sa = ifi->ifi_ntmaddr) != NULL)
			printf("network mask: %s\n", Sock_ntop_host(sa, sizeof(*sa)));		
		server_info_list[idx].netmask = (struct sockaddr*) malloc(sizeof(struct sockaddr));
		server_info_list[idx].netmask = sa;

		//finding the subnet address
		server_addr_in = (struct sockaddr_in*) server_info_list[idx].IP_addr;
		network_mask_in = (struct sockaddr_in*) server_info_list[idx].netmask;
		
		subnet_addr_in = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
		subnet_addr_in->sin_addr.s_addr = server_addr_in->sin_addr.s_addr & network_mask_in->sin_addr.s_addr;
		
		server_info_list[idx].subnet_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
		server_info_list[idx].subnet_addr = (struct sockaddr*) subnet_addr_in ; 
	
		printf("IP address is: %s\n", Sock_ntop_host(server_info_list[idx].IP_addr, sizeof(struct sockaddr*)) );
		printf("Network mask is: %s\n", Sock_ntop_host(server_info_list[idx].netmask, sizeof(struct sockaddr*)) );
		printf("Subnet mask is: %s\n", inet_ntoa(subnet_addr_in->sin_addr) );
		printf("\n");

		idx++;
		no_of_interface++;
		printf("\n--------------------------------%d\n", no_of_interface);

	}

	//IO multiplexing for sockfd
	

	FD_ZERO(&rset);
	while(1){
		
		idx=0;
		for(idx = 0 ; idx < no_of_interface; idx++){
			FD_SET(server_info_list[idx].sockfd, &rset);
			if(server_info_list[idx].sockfd > maxfd){
				maxfd = server_info_list[idx].sockfd;
			}
		}
		maxfdp1 = maxfd + 1;
		select( maxfdp1, &rset, NULL, NULL, NULL);

		for(idx = 0; idx < no_of_interface; idx++){

			if(FD_ISSET(server_info_list[idx].sockfd, &rset )){

				printf("Socket number %d received request from client.\n", server_info_list[idx].sockfd);
				recvfrom_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
				ret = recvfrom( server_info_list[idx].sockfd, buf_str, 64, 0, recvfrom_addr, &recvfrom_len);

				printf("Connection request recieved from %s\n", Sock_ntop_host(recvfrom_addr, recvfrom_len));
				buf_len = strlen(buf_str);
				buf_str[ret] = '\0';
			//	printf("Message from client is:\n");
			//	printf("%s\n", buf_str);

				if ( (childpid = Fork()) == 0 ){
					printf(" Child process created\n");	
					for( i=0; i< no_of_interface; i++){
						if(server_info_list[i].sockfd != server_info_list[idx].sockfd ){
							close(server_info_list[i].sockfd);
						}
					}	
					i=0;
	
					child_sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
					child_server_addr_in = (struct sockaddr_in*) malloc(sizeof (struct sockaddr_in));
					child_server_addr_in->sin_family = AF_INET;
					child_server_addr_in->sin_port = htons(0);
					child_server_addr_in->sin_addr.s_addr = ((struct sockaddr_in*)(server_info_list[idx].IP_addr))->sin_addr.s_addr;
					child_server_addr = (struct sockaddr*)child_server_addr_in;
					Bind(child_sockfd, child_server_addr, sizeof(*child_server_addr));
						
					on=1;	
					if( ( ret = setsockopt(child_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on) ) ) < 0 ){
                         			printf("setsockopt SO_REUSEADDR failed\n");
					}

					if( (ret = getsockname(child_sockfd, child_server_addr, &child_server_addr_len)) == -1){
						printf("getsockname failed\n");
					}
	
	
					child_port = ((struct sockaddr_in*)child_server_addr)->sin_port;
					
					sprintf( child_port_str, "%d", child_port);

					sendto( server_info_list[idx].sockfd, child_port_str, 16, 0, recvfrom_addr, recvfrom_len);
					printf(" New server port is %d\n", child_port);
					
					close(server_info_list[idx].sockfd);

					Connect( child_sockfd, recvfrom_addr, recvfrom_len);

					ret = -1;
					ret = recv( child_sockfd, requested_file_name, 32, 0);

				//	printf(" Name of file requested is: %s\n", requested_file_name);

					for(i = 0 ; i< strlen(requested_file_name); i++){
				//		printf("%d : %c\n", i , requested_file_name[i]);
					}
					requested_file_name[i] = '\0';
					
					printf(" Name of file requested is: %s\n", requested_file_name);
					FILE *fpr;
					
					fpr = fopen( requested_file_name, "rb");
					if( fpr == NULL){
						printf(" File named %s doesn't exist on server\n", requested_file_name);
						printf("%s\n", strerror(errno));
						exit(1);
					}
				
					printf(" Sending file to client...\n");

					fseek(fpr, 0L, SEEK_END);
					file_size = ftell(fpr);
					fseek(fpr, 0L, SEEK_SET);

					sprintf( file_size_str, "%d", file_size);
					
					//sending file size
					Send( child_sockfd, file_size_str, strlen(file_size_str), 0);
				//	printf("child socket fd is %d\n", child_sockfd);			
					
					int return_file_buf_size, file_buf_size;
					struct buf_ele file_buf[ (file_size/PAYLOAD_SIZE) + 2];

					read_file(fpr, file_buf, file_size, &return_file_buf_size);
					
					file_buf_size = return_file_buf_size;
					send_file( file_buf, file_buf_size, child_sockfd, recvfrom_addr);

					close( child_sockfd);
					close(server_info_list[idx].sockfd);
					printf("Child exited at server\n");
					exit(0);
				}
				else{
				//	close(server_info_list[idx].sockfd);
				}
				

			}
		}
	printf("waiting for request\n");
	
	}

	fclose(fp);
	if (read_file_line)
		free(read_file_line);
	

}








