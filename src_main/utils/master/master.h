#ifndef MASTER_H
#define MASTER_H

#include "shared.h"

typedef struct server_s server_t;

typedef enum {waiting, inuse} pingstate;
struct server_s
{
	server_t		*prev;
	server_t		*next;
	struct sockaddr_in	ip;
	unsigned short	port;
	unsigned int	queued_pings;
	unsigned int	heartbeats;
	unsigned long	last_heartbeat;
	unsigned long	last_ping;
	unsigned char	shutdown_issued;
	int		validated; // FS: Changed from unsigned char to int
	char	gamename[MAX_GAMENAME_LEN]; // FS: Added
	char	challengeKey[64]; // FS: For gamespy encode type 0 validation
	char	hostnameIp[MAX_DNS_NAMELEN+1]; // FS: If server was added from a list
};

#if 0
struct querySocket_s
{
	SOCKET	socket;
	pingstate state;
};
#endif

void RunFrame (void);
void ExitNicely (void);
void DropServer (server_t *server);
int  AddServer (struct sockaddr_in *from, int normal, unsigned short queryPort, char *gamename, char *hostnameIp); // FS: Added queryPort, gamename, and hostnameIp
void QueueShutdown (struct sockaddr_in *from, server_t *myserver);
void Ack (struct sockaddr_in *from, char *dataPacket);
int  HeartBeat (struct sockaddr_in *from, char *data);
void ParseCommandLine(int argc, char *argv[]);
int ParseResponse (struct sockaddr_in *from, char *data, int dglen);

/* FS: Gamespy specific helper functions */
void SendGamespyListToClient (int socket, char *gamename, struct sockaddr_in *from, int basic);
void Close_TCP_Socket_On_Error (int socket, struct sockaddr_in *from);
void Gamespy_Parse_List_Request(char *clientName, char *querystring, int socket, struct sockaddr_in *from);
int Gamespy_Challenge_Cross_Check(char *challengePacket, char *validatePacket, int rawsecurekey);
void Gamespy_Parse_UDP_Packet(int socket, struct sockaddr_in *from);
void Gamespy_Parse_TCP_Packet (int socket, struct sockaddr_in *from);
int UDP_OpenSocket (int port);

#ifdef QUAKE2_SERVERS
void SendServerListToClient (struct sockaddr_in *from);
#endif /* QUAKE2_SERVERS */

void Add_Servers_From_List(char *filename); // FS
void Check_Port_Boundaries (void); // FS
struct in_addr Hostname_to_IP (struct in_addr *server, char *hostnameIp); // FS: For serverlist

#endif /* MASTER_H */
