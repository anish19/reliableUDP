#include <stdio.h>
#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"
#include <stdlib.h>
//#include <netinet/in.h>

int on_same_host(struct sockaddr* cli, struct sockaddr* srv){

	printf("server addr is %s", cli);
	return 1;
}


int main(int argc, char* argv[]){
	FILE * fp;
	char read_file_line[128];
	size_t len = 128;
	
	int server_port = 0;
	int i =0, ret = 0;
	struct  in_addr server_addr, client_host_addr;
	struct ifi_info* ifi;
	struct ifi_info* ifihead;
	u_char *ptr;
	struct sockaddr *sa;
	struct sockaddr_in *sa_in;
	int no_client_host = 0;		//number of client hosts

	fp = fopen(argv[1], "r");	
	
	if (fp == NULL){
		printf("client.in cannot be opened\nEnter correct file name \n");
		exit(EXIT_FAILURE);
	}

	while (fgets(read_file_line, len, fp) != NULL) {
		printf("%s", read_file_line);
		if(i == 0){
			int ret = inet_pton(AF_INET, read_file_line, &server_addr);
			if(ret == -1)
				printf("Given IP address is not valid IPv4 address.\nProvide a valid IPv4 address\n");
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

//****printf("sa is : %s\n", Sock_ntop_host(sa, sizeof(*sa)) );	

		if((sa = ifi->ifi_ntmaddr) != NULL)
			printf("network mask: %s\n", Sock_ntop_host(sa, sizeof(*sa)));		
			
		no_client_host++;		//increment the count of number of host	
	
	}

	printf("\n");
	for( ifi = ifihead; ifi != NULL; ifi = ifi->ifi_next){
		if( (sa = ifi->ifi_addr) != NULL){
		//	if( ret = on_same_host(sa, server_addr) )
			printf("client hosts are : %s\n", sa);
		//	for(i = 0 ; i < 32 ; i++){
				ret = inet_pton( AF_INET, Sock_ntop_host(sa, sizeof(*sa)), &client_host_addr); 
			//	printf("The size of client address is %d\n", sizeof(client_host_addr));
				ret = memcmp( &client_host_addr, &server_addr, 4);
				printf("the value of ret is %d\n", ret);
	
				printf("byte at cli are %lu\n", client_host_addr.s_addr);
				printf("byte at srv are %lu\n", server_addr.s_addr);
			//	if(client_host_addr->s_addr != server_addr->s_addr){
			//		printf("%s and %s are diff\n", Sock_ntop_host(sa, sizeof(*sa)),server_addr);
			//		break;
			//	}
		//	}
		}
	
	}
	
	free_ifi_info_plus(ifihead);

	fclose(fp);
	if (read_file_line)
		free(read_file_line);
	

}
