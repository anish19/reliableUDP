#include <stdio.h>
#include <string.h>
#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"
#include <stdlib.h>
#include <string.h>
//#include <netinet/in.h>

/*
int on_same_subnet(char server[15], char client[15], char nmsk_str[15]){
	int network_len = 0, count =0;
	int nmsk[4];
	char temp[3];
	int i=0, j=0;

	for(i = 0; i < 4 ; i++){
		for(j = 0; j < 3 ; j++){
			temp[j] = nmsk_str[4*i+j];
		}
		nmsk[i] = atoi(temp);
	}

	for(i=0; i<4 ;i++){
		count = 0;
		nmsk[i] = nmsk[i] << 24;
		while( nmsk[i] != 0 ){
			nmsk[i]<<1;
			count++;
		}
		network_len+=count;
	}
	
	for(i = 0; i < network_len; i++)
}
*/

int dg_cli_echo(int sockfd, const void* buf, int buf_len, struct sockaddr *server){
	Sendto(sockfd, buf, 16, 0, server, sizeof(server));
	return 1;
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

	struct sockaddr *IPclient_addr, *IPserver_addr;
	struct sockaddr_in *IPclient_addr_in, *IPserver_addr_in;
	
	int sockfd; //socket fd for client
	struct sockaddr *local_addr = NULL;
	int local_addr_len = 0;

	char *buf = NULL;
	int buf_len = 0;
	char buf_str[16] = "random text";

	fp = fopen(argv[1], "r");	
	
	if (fp == NULL){
		printf("client.in cannot be opened\nEnter correct file name \n");
		exit(EXIT_FAILURE);
	}

	while (fgets(read_file_line, len, fp) != NULL) {
		printf("%s", read_file_line);
		if(i == 0){
		//	ret = inet_pton(AF_INET, read_file_line, &server_addr_in.sin_addr);
		//	ret = inet_pton(AF_INET, read_file_line, &server_in_addr);
		
			inet_aton(read_file_line, &server_in_addr);
			server_addr_in.sin_addr = server_in_addr;

			IPserver_addr_in = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
			IPserver_addr_in->sin_family = AF_INET;
			IPserver_addr_in->sin_addr = server_in_addr;
			IPserver_addr_in->sin_port = 60810;
			IPserver_addr = (struct sockaddr*) IPserver_addr_in;
			printf("server address from file is %s\n ", Sock_ntop_host(IPserver_addr, sizeof(IPserver_addr)));
			if(ret == -1)
				printf("Given IP address is not valid IPv4 address.\nProvide a valid IPv4 address\n");
			printf("\n");
			for(i=0; i < 16; i++)
				server_str_addr[i] = read_file_line[i];
		}
		if(i == 1)
			server_port = atoi(read_file_line);
		i++;
	}

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
	buf = (char*) malloc(sizeof(char)*buf_len);

//	ret = dg_cli_echo(sockfd,(void*) buf, buf_len, IPserver_addr);
//	ret = sendto( sockfd, buf_str, buf_len, 0, IPserver_addr, sizeof(IPserver_addr));

	ret = send( sockfd, buf_str, buf_len, 0);	

	if( ret == -1){
		printf("sendto failed\n ");
	}
	else if (ret < buf_len){
		printf("sendto only sent %d bytes\n", ret);
	}

	printf("end of process\n");

	//close fp and free read_file_line earlier
	close(sockfd);
	fclose(fp);
	if (read_file_line)
		free(read_file_line);
	

}
