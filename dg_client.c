#include <stdio.h>
#include <string.h>
#include <time.h>
#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"
#include "dg_hdr.h"
#include <stdlib.h>
#include <string.h>
#include "unprtt.h"
#include <setjmp.h>
//#include <netinet/in.h>

static struct rtt_info rttinfo;
static int rttinit = 0;
static struct msghdr msgsend, msgrecv;
static struct dg_hdr sendhdr, recvhdr;
//static void sig_alrm(int signo);
//static sigjmp_buf jmpbuf;
int client_window_size = 5;
int seed,  mu;
float p_loss;

void recv_file( FILE *fp , int sockfd, struct sockaddr* server_addr, int file_size){

	char file_data[PAYLOAD_SIZE];
	int written_ctr = 0, ctr = 0 , n=0, recv_data_size;
	int no_cwr;		//no of complete write reqd, 
	int last_write_size;			//bytes_written_in_last_write;
	struct iovec iovsend[1], iovrecv[2];

	int file_buf_size = (file_size/PAYLOAD_SIZE) + 1;
	struct buf_ele file_buf[file_buf_size];
		
	no_cwr = file_size/PAYLOAD_SIZE;
	last_write_size = file_size - (no_cwr*PAYLOAD_SIZE);
	int max_window_size = client_window_size;
	int idx = 0, i=0, window_empty = max_window_size;
	recvhdr.seq = 0;
	int random_no;
		
	for (i = 0; i < file_buf_size; ++i)
	{
		file_buf[i].sent = 0;
		file_buf[i].ack = 0;
		file_buf[i].seq = 0;
		file_buf[i].ts = 0;
		file_buf[i].drop = 0;
		file_buf[i].data_size = 0;
	}
	i=0;
	srand(seed);
	
	while(1){

		if(rttinit == 0){
			rtt_init(&rttinfo);
			rttinit = 1;
			rtt_d_flag = 1;
		}
		
		iovrecv[0].iov_base = (void*)&recvhdr;
		iovrecv[0].iov_len = sizeof(struct dg_hdr);
		iovrecv[1].iov_base = file_data;
		iovrecv[1].iov_len = PAYLOAD_SIZE;
		msgrecv.msg_iov = iovrecv;
		msgrecv.msg_iovlen = 2;
		
//		idx = recvhdr.seq;	//setting idx to print only
//	printf("\nwaiting to rcv %d\n", idx+1);
		n = Recvmsg(sockfd, &msgrecv, 0);
	printf("\nrcvd %d\n", recvhdr.seq);

		idx = recvhdr.seq-1;	//setting actual idx value
		file_buf[idx].drop = 0;
		
		//drop here
		random_no = rand()%10 + 1;
		printf("random no is %d\n", random_no);
		printf("p_loss is %f\n", p_loss);
		if( random_no <= (p_loss*10) ){
			file_buf[idx].drop = 1;
			printf("dropped packet no: %d\n", idx+1);
		}
		
		if( file_buf[idx].drop != 1){	//if not dropped
			
			file_buf[idx].seq 	= 	recvhdr.seq;
			file_buf[idx].ts 	= 	recvhdr.ts;
			file_buf[idx].ack 	= 	0;	//ack not sent yet. will set to 1 when ack sent

			//reading payload
			i=0;
			do{
				file_buf[idx].data[i]	=	file_data[i];
				if(file_data[i] == '\0')
					break;
				i++;
			}while(i < PAYLOAD_SIZE);
		
		

			i=0;
			while(1){
			
				if( !file_buf[i].drop ){	//if not dropped
					if(i == idx){
						sendhdr.seq = file_buf[i].seq + 1;
						break;
					}
				}
				else{
					sendhdr.seq = i+1;
					break;		
				}
				i++;
			}
		
			sendhdr.ts = recvhdr.ts;
			sendhdr.window_empty = window_empty--;
			msgsend.msg_namelen = sizeof(*server_addr);
			iovsend[0].iov_base = (void*)&sendhdr;
			iovsend[0].iov_len = sizeof(struct dg_hdr);
			msgsend.msg_iov = iovsend;
			msgsend.msg_iovlen = 1;
	 
			Sendmsg(sockfd, &msgsend, 0);
			file_buf[idx].ack = 1;

			printf("Server address is : %s\n", Sock_ntop_host(server_addr, sizeof(struct sockaddr*)) );
		
			printf("Port no of server is: %d\n", ((struct sockaddr_in*)server_addr)->sin_port );


			printf( "Msg with seq no %d is:\n%s\n", recvhdr.seq ,iovrecv[1].iov_base);
//			printf( "Msg with seq no %d is rcvd\n", recvhdr.seq);
			printf("header of ack for seqno %d is:%d\n", recvhdr.seq, sendhdr.seq);
			window_empty++;
		
			printf("ack no sent is %d\n", sendhdr.seq);
		
		}
		
		if(file_buf[file_buf_size-1].ack == 1)
			break;
	}


/*
		if(recv_data_size <= 0){
			break;
		}

		n  = fwrite( file_data, sizeof(char), sizeof(file_data)/sizeof(char), fp);
	
		ctr++;
		if(ctr == no_cwr)
			break;
	//	written_ctr += n;
	//	if(written_ctr >= file_size)
	//		break;
	}
	
	recv_data_size = dg_recv_packet( sockfd, server_addr, file_data);
	file_data[last_write_size] = '\0';
	n  = fwrite( file_data, sizeof(char), last_write_size/sizeof(char), fp);
*/
printf("outside while\n");
}


int on_same_subnet( struct sockaddr_in server_addr_in, struct sockaddr_in* client_addr_in, struct sockaddr_in* mask){
	uint32_t client_addr = htonl(client_addr_in->sin_addr.s_addr);
	uint32_t server_addr = htonl(server_addr_in.sin_addr.s_addr);
	uint32_t mask_addr = htonl(mask->sin_addr.s_addr);
	if((client_addr & mask_addr) == (server_addr & mask_addr))
		return 1;
	else
		return 0;
}


int main(int argc, char* argv[]){
	FILE * fp;
	char read_file_line[128];
	size_t len = 128;
	
	int server_port = 0;
	int i =0, ret = 0, on = 1;
	char server_str_addr[15], client_str_addr[15];
	struct in_addr server_in_addr;
	struct sockaddr_in *client_host_addr_in, server_addr_in;
	struct sockaddr *client_addr;
	
	client_host_addr_in = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));

	struct ifi_info* ifi = NULL;
	struct ifi_info* ifihead = NULL;
	u_char *ptr = NULL;
	struct sockaddr *sa = NULL;
	struct sockaddr_in *sa_in = NULL;
	
	int no_client_host = 0;		//number of client hosts

	char loopback_str[9] = "127.0.0.1";
	int server_is_loopback = 1;
	
	char network_mask_str[15];
	struct sockaddr_in *subnet_mask_in;
	int server_is_local = 0;

	subnet_mask_in = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
	
	struct sockaddr *IPclient_addr, *IPserver_addr;
	struct sockaddr_in *IPclient_addr_in, *IPserver_addr_in;
	
	IPserver_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
	int sockfd; //socket fd for client
	struct sockaddr *local_addr = NULL;
	int local_addr_len = -1;

	char *buf = NULL;
	int buf_len = 0;
	char buf_str[] = "Hi server... ";

	int new_server_port;
	char new_server_port_str[16];
	struct sockaddr *recvfrom_addr;
	struct sockaddr_in *recvfrom_addr_in;
	int recvfrom_len = -1;

//	char file_data[512];
	char requested_file_name[32], file_size_str[15];
	int requested_file_name_len = 0, file_size, payload_size;
	FILE *fpw;

	
	char write_file_name[16];
	
	fp = fopen(argv[1], "r");	
	
	if (fp == NULL){
		printf("client.in cannot be opened\nEnter correct file name \n");
		exit(EXIT_FAILURE);
	}

	while (fgets(read_file_line, len, fp) != NULL) {
//		printf("%s", read_file_line);
		if(i == 0){
			inet_aton(read_file_line, &server_in_addr);
			server_addr_in.sin_addr = server_in_addr;

			IPserver_addr_in = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
			IPserver_addr_in->sin_family = AF_INET;
			IPserver_addr_in->sin_addr = server_in_addr;
			IPserver_addr_in->sin_port = 60810;
			IPserver_addr = (struct sockaddr*) IPserver_addr_in;
			printf("Server address is :  %s\n", Sock_ntop_host(IPserver_addr, sizeof(IPserver_addr)));
			if(ret == -1)
				printf("Given IP address is not valid IPv4 address.\nProvide a valid IPv4 address\n");
			for(i=0; i < 16; i++)
				server_str_addr[i] = read_file_line[i];
			i = 0;
		}
		if(i == 1){
			server_port = atoi(read_file_line);
			printf("Server port is : %d\n", server_port);
		}
		
		if(i == 2){
			//requested_file_name = (char*) malloc(requested_file_name_len);
			for(i = 0; i < 32; i++){
				if(read_file_line[i]=='\n')
					break;
				requested_file_name[i] = read_file_line[i];
				requested_file_name_len++;
			}
			printf("Requested file is : %s\n", requested_file_name);
			i = 2;			
		}
		
		if(i == 3){
			client_window_size = atoi(read_file_line);
			printf("Client Window size is : %d\n", client_window_size);
		}

		if(i == 4){
			seed = atoi(read_file_line);
			printf("seed value is : %d\n", seed);
			
		}

		if(i == 5){
			char *endptr;
			printf("p_loss string is %s\n", read_file_line);
			p_loss = strtof(read_file_line, NULL);
			printf("Probality of loss is : %f\n", p_loss);
		}
		
		if(i == 6){
			mu = atoi(read_file_line);
			printf("mu is : %d\n", mu);
		}

		i++;
		
	}
	printf("\n");
	i=0;
	printf("File to be requested for is %s\n", requested_file_name);
	printf("File name len is %d\n", requested_file_name_len);

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
		if ( (sa = ifi->ifi_brdaddr) != NULL)
			printf("  broadcast addr: %s\n",
		Sock_ntop_host (sa, sizeof(*sa)));
		
		if ( (sa = ifi->ifi_dstaddr) != NULL)
			printf("  destination addr: %s\n",
		Sock_ntop_host(sa, sizeof(*sa)));

		if((sa = ifi->ifi_ntmaddr) != NULL)
			printf("network mask: %s\n", Sock_ntop_host(sa, sizeof(*sa)));		
			
		no_client_host++;		//increment the count of number of host	
	
	}

	printf("\n");
	
	for(i=0;i<9;i++){
		if(server_str_addr[i] != loopback_str[i]){
			server_is_loopback = 0;
			break;
		}
	}
	
	IPclient_addr_in = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
/********************************************************/
/********************************************************/
/********************************************************/
/********************************************************/
/********************************************************/
/********************************************************/
/***************************SET LOOPBACK IP**************/
/********************************************************/
/********************************************************/
/********************************************************/
/********************************************************/
/********************************************************/
/********************************************************/
/********************************************************/
/********************************************************/
	//if server is loopback
	if(server_is_loopback){
	//	ret = inet_pton(AF_INET, loopback_str, &IPclient_addr_in.sin_addr);
		ret = inet_pton(AF_INET, loopback_str, &(IPclient_addr_in->sin_addr));
	//	IPclient_addr_in = (struct sockaddr_in) IPclient;
		printf("server is loopback.\n");
		if(ret == -1){
			printf("cannot set loopback client address\n");
		}
	}
	else{
		for( ifi = ifihead; ifi != NULL; ifi = ifi->ifi_next){
			printf("\n");
			if( (sa = ifi->ifi_addr) != NULL){
				strcpy(client_str_addr, sock_ntop(sa, sizeof(*sa))); 
				printf("client addr is %s\n", client_str_addr);
				client_host_addr_in = (struct sockaddr_in*) sa;
			}
			
			if((sa = ifi->ifi_ntmaddr) != NULL){
				strcpy(network_mask_str, sock_ntop(sa, sizeof(*sa)));
				subnet_mask_in = (struct sockaddr_in*) sa;
			}

			if(on_same_subnet(server_addr_in, client_host_addr_in, subnet_mask_in)){
				ret = inet_pton(AF_INET, client_str_addr, &(IPclient_addr_in->sin_addr));
				if(ret == -1){
					printf("cannot set local client address\n");
				}
				printf("server is local\n");
				client_addr = (struct sockaddr*) client_host_addr_in;
				printf("client on same subnet is %s\n", sock_ntop(client_addr, sizeof(client_addr)));
				server_is_local = 1;

			}
			else{
			
				client_addr = (struct sockaddr*) client_host_addr_in;
				printf("client not on same subnet is %s\n", sock_ntop(client_addr, sizeof(client_addr)));
			}
		}

		if(!server_is_local){
			sa = ifihead->ifi_next->ifi_addr;
			strcpy(client_str_addr, sock_ntop(sa, sizeof(*sa))); 
			ret = inet_pton(AF_INET, client_str_addr, &(IPclient_addr_in->sin_addr));
			if(ret == -1){
				printf("cannot set arbitrary client address\n");
			}
			printf("server is neither loopback nor local\n");
		}
				
	}
	
	IPclient_addr_in->sin_port = htons(0);
	IPclient_addr = (struct sockaddr*)IPclient_addr_in;
	printf("IPclient is %s\n", sock_ntop(IPclient_addr, sizeof(IPclient_addr)));
	printf("IPserver is %s\n", sock_ntop(IPserver_addr, sizeof(IPserver_addr)));
	printf("\n");
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd == -1){
		printf("cound not create client socket\n");
	}


	if( ( ret = bind(sockfd, IPclient_addr, sizeof(*IPclient_addr)) == -1)){
		printf("Could not bind client IP address to socket\n");
	}
	if(server_is_loopback || server_is_local){
		if( ( ret = setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, (void*)&on, sizeof(on) ) ) < 0 ){
			printf("setsockopt SO_DONTROUTE failed\n");
		}
		if( ( ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on) ) ) < 0 ){
			printf("setsockopt SO_REUSEADDR failed\n");
		}
	}
	else{
		if( ( ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on) ) ) < 0 ){
			printf("setsockopt SO_REUSEADDR failed\n");
		}
	}

	local_addr_len = sizeof(*IPclient_addr);
	local_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
	if( (ret = getsockname(sockfd, local_addr, &local_addr_len) ) == -1 ){
		printf("getsockname failed\n");
		close(sockfd);
	}
	printf("local addr len is %d\n", local_addr_len);
	printf("The local client address after binding to fd is %s\n", sock_ntop(local_addr, sizeof(local_addr)) );

	free_ifi_info_plus(ifihead);

	printf("sockfd is %d\n",sockfd);
	printf("IPserver is %s\n", Sock_ntop_host(IPserver_addr, sizeof(struct sockaddr)));

	//connect to server
	if( (ret = connect(sockfd, IPserver_addr, sizeof(*IPserver_addr)) ) < 0 ){
		printf("connect to server failed\n");	
	}
	buf_len = strlen(buf_str);

	ret = send( sockfd, requested_file_name, requested_file_name_len, 0);	
printf("ret from send is %d\n", ret);

	if( ret == -1){
		printf("sendto failed\n ");
	}
//	else if (ret < buf_len){
//		printf("only sent %d bytes of file name\n", ret);
//	}

	ret = -1;
	
		ret = recv( sockfd, new_server_port_str, 16, 0);
		if(ret > -1)
			printf("port no not rcvd\n");
	

	new_server_port = atoi(new_server_port_str);
	printf("new server port is %d\n", new_server_port);	
	recvfrom_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
	memcpy(recvfrom_addr, IPserver_addr, 16);
	recvfrom_addr_in = (struct sockaddr_in*) recvfrom_addr;
	recvfrom_addr_in->sin_port = htons(new_server_port);
	recvfrom_addr = (struct sockaddr*)recvfrom_addr_in;
	

	//connect to new server port
	if( (ret = connect( sockfd, recvfrom_addr, sizeof(*recvfrom_addr)) ) == 0){
		printf("new connect successful\n");
		Send( sockfd, requested_file_name, requested_file_name_len, 0); 
		printf("Request for file sent to server\n");
	}
	else
		printf("new connect failed\n");

	Recv( sockfd, file_size_str, 15, 0);
	file_size = atoi(file_size_str);
	printf("The size of file to recv is: %d\n", file_size);
	
	
//	printf("Recieving file from server...\n");
	
//	time_t mytime;
//	mytime = time(NULL);
//	sprintf( write_file_name,"%s",ctime(&mytime));
//	write_file_name[16] = '\0';
//	for( i = 0 ; i < 32; i++){
//		if(requested_file_name[i] == '\0')
//			break;
//		write_file_name[i] = ; 

//	}

	write_file_name[0] = 'o';
	write_file_name[1] = 'u';
	write_file_name[2] = 't';
	write_file_name[3] = 'p';
	write_file_name[4] = 'u';
	write_file_name[5] = 'u';
	write_file_name[6] = '\0';

	//opening file for writing
	fpw = fopen( write_file_name, "wb");
	if(fpw == NULL ){
		printf("Unable to access file for writing\n");
	}

	recv_file( fpw, sockfd, recvfrom_addr, file_size);

	printf("end of process\n");

	//close fp and free read_file_line earlier
	close(sockfd);
	fclose(fp);
	if (read_file_line)
		free(read_file_line);
	
return 1;
}
