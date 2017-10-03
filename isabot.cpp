#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdarg.h> 

int conn;
char sbuf[512];

void raw(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sbuf, 512, fmt, ap);
    va_end(ap);
    printf("<< %s", sbuf);
    write(conn, sbuf, strlen(sbuf));
}

int main() {
    
    char *nick = "test";
    char *channel = NULL;
    char *host = "chat.freenode.net";
    char *port = "6667";
    
    char *user, *command, *where, *message, *sep, *target;
    int i, j, l, sl, o = -1, start, wordcount;
    char buf[513];
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo(host, port, &hints, &res);
    conn = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    connect(conn, res->ai_addr, res->ai_addrlen);
    
    raw("USER %s 0 0 :%s\r\n", nick, nick);
    raw("NICK %s\r\n", nick);
    raw("JOIN %s\r\n", nick);
    
    while ((sl = read(conn, sbuf, 512))) {
    	printf(">> %s", sbuf);
        for (i = 0; i < sl; i++) {
        	printf(">> %s", "in for");
            o++;
            buf[o] = sbuf[i];
            if ((i > 0 && sbuf[i] == '\n' && sbuf[i - 1] == '\r') || o == 512) {
                buf[o + 1] = '\0';
                l = o;
                o = -1;
                
                printf(">> %s", buf);
                
                if (!strncmp(buf, "PING", 4)) {
                    buf[1] = 'O';
                    raw(buf);
                } else if (buf[0] == ':') {
                    wordcount = 0;
                    user = command = where = message = NULL;
                    for (j = 1; j < l; j++) {
                        if (buf[j] == ' ') {
                            buf[j] = '\0';
                            wordcount++;
                            switch(wordcount) {
                                case 1: user = buf + 1; break;
                                case 2: command = buf + start; break;
                                case 3: where = buf + start; break;
                            }
                            if (j == l - 1) continue;
                            start = j + 1;
                        } else if (buf[j] == ':' && wordcount == 3) {
                            if (j < l - 1) message = buf + j + 1;
                            break;
                        }
                    }
                    
                    if (wordcount < 2) continue;
                    
                    if (!strncmp(command, "001", 3) && channel != NULL) {
                        raw("JOIN %s\r\n", channel);
                    } else if (!strncmp(command, "PRIVMSG", 7) || !strncmp(command, "NOTICE", 6)) {
                        if (where == NULL || message == NULL) continue;
                        if ((sep = strchr(user, '!')) != NULL) user[sep - user] = '\0';
                        if (where[0] == '#' || where[0] == '&' || where[0] == '+' || where[0] == '!') target = where; else target = user;
                        printf("[from: %s] [reply-with: %s] [where: %s] [reply-to: %s] %s", user, command, where, target, message);
                        //raw("%s %s :%s", command, target, message); // If you enable this the IRCd will get its "*** Looking up your hostname..." messages thrown back at it but it works...
                    }
                }
                
            }
        }
        
    }
    
    return 0;
    
}

/*#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

enum tError {
    eOK = 0,
    eARG_NUMBER,
    eARG,
    eADDR,
    eSOCK,
    eDNS,
    eCONN,
    eSEND,
    eRECV,
    eUNKNOWN,
    eMESG
};

string address = "";
string channels = "";
string syslog = "";
string lights = "";
string port = "";

const char *eMSG[]{
        "",
        "Error: Invalid number of arguments. Rerun with -h for help.\n",
        "Error: Invalid argument. Rerun with -h for help.\n",
        "Error: Invalid hostname. Not valid IPv4 or IPv6 address.\n",
        "Error: Can't create socket\n",
        "Error: Get host by name error.\n",
        "Error: Can't create connection.\n",
        "Error: Problem while sending message to server.\n",
        "Error: Problem while receiving message from server.\n",
        "Error: Unknown message received from server.\n",
        "Error: Something unexpected happened.\n"
};

void printError(int eCode);
void printHelp();
bool is_ipv4_address(const string& str);
bool is_ipv6_address(const string& str);
int connectTo(const char* addr, const char* port, int *sock);
int talkTo(int *s);
bool sendData(const char *msg, int *s);
void msgHandel(char * buf, int *s);
bool charSearch(char *toSearch, const char *searchFor);
void sendPong(char *buf, int *s);
char * timeNow();

/**
 * Entry point
 * @param  argc 	number of command line arguments
 * @param  argv 	command line arguments
 * @return      	error code
 */
/*int main (int argc, char *argv[]) {

	int eCode = eOK;

	if (argc < 3) {
		if (argc == 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "-help") == 0) || (strcmp(argv[1], "--help") == 0))) {
			printHelp();
			return eCode;
		} else {
			eCode = eARG_NUMBER;
        	printError(eCode);
        	return eCode;
		}	
	}

	if ((argc > 7) || argc % 2 == 0) {
		eCode = eARG_NUMBER;
        printError(eCode);
        return eCode;
	}

	if (argc >= 3) {
		address = string(argv[1]);
		channels = argv[2];
		if (argc >= 5) {
			if (strcmp(argv[3], "-s") == 0) {
				syslog = argv[4];
			} else if (strcmp(argv[3], "-l") == 0) {
				lights = argv[4];
			} else {
				eCode = eARG;
				printError(eCode);
				return eCode;
			}
		} else if (argc == 7) {
			if ((strcmp(argv[3], "-s") == 0) && (strcmp(argv[5], "-l") == 0)) {
				syslog = argv[4];
				lights = argv[6];
			} else if ((strcmp(argv[3], "-l") == 0) && (strcmp(argv[5], "-s") == 0)) {
				lights = argv[4];
				syslog = argv[6];
			} else {
				eCode = eARG;
				printError(eCode);
				return eCode;
			}	
		}
	}

	unsigned index;
    if ((index = address.find(":")) != (unsigned) string::npos) {
        port = address.substr(index + 1);
        address = address.substr(0, index);
    } else {
    	port = "6667";
    }

	int sock;
    eCode = connectTo(address.c_str(), port.c_str(), &sock);
    if (eCode != eOK) {
        printError(eCode);
        return eCode;
    }

    talkTo(&sock);

    return eCode;

}	

/**
 * Prints error code message
 * @param eCode error code
 */
/*void printError(int eCode) {
    if (eCode < eOK || eCode > eUNKNOWN)
        eCode = eUNKNOWN;

    cerr << eMSG[eCode] << endl;
    exit(eCode);
}

/**
* Prints help
*/
/*void printHelp() {
    cout << "------------------------------------------------------------------------------------\n";
    cout << "isabot HOST[:PORT] CHANNELS [-s SYSLOG_SERVER] [-l HIGHLIGHT] [-h|--help]\n";
	cout << "	HOST je název serveru (např. irc.freenode.net)\n";
	cout << "	PORT je číslo portu, na kterém server naslouchá (výchozí 6667)\n";
	cout << "	CHANNELS obsahuje název jednoho či více kanálů, na které se klient připojí (název kanálu je zadán včetně úvodního # nebo &; v případě více kanálů jsou tyto odděleny čárkou)\n";
    cout << "   -s SYSLOG_SERVER je ip adresa logovacího (SYSLOG) serveru\n";
    cout << "   -l HIGHLIGHT seznam klíčových slov oddělených čárkou (např. \"ip,tcp,udp,isa\")\n";
    cout << "example:\n";
    cout << "	isabot irc.freenode.net:6667 \"#ISAChannel,#IRC\" -s 192.168.0.1 -l \"ip,isa\"\n";
    cout << "------------------------------------------------------------------------------------\n";
}

/**
 * Check if given address is valid IPv4 address
 * @param  str string containing address
 * @return     true, if str is valid IPv4 address, false otherwise
 */
/*bool is_ipv4_address(const string& str) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, str.c_str(), &(sa.sin_addr))!=0;
}

/**
 * Check if given address is valid IPv6 address
 * @param  str string containing address
 * @return     true, if str is valid IPv6 address, false otherwise
 */
/*bool is_ipv6_address(const string& str) {
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, str.c_str(), &(sa.sin6_addr))!=0;
}

/**
 * Connects to given IPv4/IPv6 address and return socket in parameter
 * @param  addr IPv4/IPv6 address to connect to
 * @param  sock connected socket is returned here
 * @return      error code
 *
 * Function contains code from getaddinfo(3) man page
 */
/*int connectTo(const char* addr, const char* port, int *sock) {

	struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
//    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
 //   hints.ai_flags = 0;
 //   hints.ai_protocol = 0;          /* Any protocol */

 /*   s = getaddrinfo(addr, port, &hints, &result);
    if (s != 0) {
        return eDNS;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

       if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

/*       close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
/*        return eCONN;
    }

    freeaddrinfo(result);

    *sock = sfd;
    return 0;

}

int talkTo(int *s) {
	//Recv some data
    int numbytes;
    char buf[100];
 
    const char* nick = "NICK testBOT\r\n";
    const char* usr = "USER guest tolmoon tolsun :Ronnie Reagan\r\n";

    int count = 0;
    while (1)
    {
        //declars
        count++;
 
        switch (count) {
            case 3:
                    //after 3 recives send data to server (as per IRC protacol)
                    sendData(nick, s);
                    sendData(usr, s);
                break;
            case 4:
                    //Join a channel after we connect, this time we choose beaker
                sendData("JOIN #ubuntu\r\n", s);
            default:
                break;
        }
 
        //Recv & print Data
        numbytes = recv(*s,buf,99,0);
        buf[numbytes]='\0';
        cout << buf;
        //buf is the data that is recived
 
        //Pass buf to the message handeler
        msgHandel(buf, s);
 
        //If Ping Recived
        /*
         * must reply to ping overwise connection will be closed
         * see http://www.irchelp.org/irchelp/rfc/chapter4.html
         */
/*        if (charSearch(buf,"PING"))
        {
            sendPong(buf, s);
        }
 
        //break if connection closed
        if (numbytes==0)
        {
            cout << "----------------------CONNECTION CLOSED---------------------------"<< endl;
            cout << timeNow() << endl;
 
            break;
        }
    }

    return 0;
}

bool sendData(const char *msg, int *s)
{//Send some data
    //Send some data
    int len = strlen(msg);
    int bytes_sent = send(*s,msg,len,0);
 
    if (bytes_sent == 0)
        return false;
    else
        return true;
}

void msgHandel(char * buf, int *s)
{
    /*
     * TODO: add you code to respod to commands here
     * the example below replys to the command hi scooby
     */
/*    if (charSearch(buf,"hi scooby"))
    {
        sendData("PRIVMSG #ubuntu :hi, hows it going\r\n", s);
    }
 
}

bool charSearch(char *toSearch, const char *searchFor)
{
    int len = strlen(toSearch);
    int forLen = strlen(searchFor); // The length of the searchfor field
 
    //Search through each char in toSearch
    for (int i = 0; i < len;i++)
    {
        //If the active char is equil to the first search item then search toSearch
        if (searchFor[0] == toSearch[i])
        {
            bool found = true;
            //search the char array for search field
            for (int x = 1; x < forLen; x++)
            {
                if (toSearch[i+x]!=searchFor[x])
                {
                    found = false;
                }
            }
 
            //if found return true;
            if (found == true)
                return true;
        }
    }
 
    return 0;
}

void sendPong(char *buf, int *s)
{
    //Get the reply address
    //loop through bug and find the location of PING
    //Search through each char in toSearch
 
    const char * toSearch = "PING ";
 
    for (unsigned i = 0; i < strlen(buf);i++)
        {
            //If the active char is equil to the first search item then search toSearch
            if (buf[i] == toSearch[0])
            {
                bool found = true;
                //search the char array for search field
                for (unsigned x = 1; x < 4; x++)
                {
                    if (buf[i+x]!=toSearch[x])
                    {
                        found = false;
                    }
                }
 
                //if found return true;
                if (found == true)
                {
                    int count = 0;
                    //Count the chars
                    for (unsigned x = (i+strlen(toSearch)); x < strlen(buf);x++)
                    {
                        count++;
                    }
 
                    //Create the new char array
                    char returnHost[count + 5];
                    returnHost[0]='P';
                    returnHost[1]='O';
                    returnHost[2]='N';
                    returnHost[3]='G';
                    returnHost[4]=' ';
 
                    count = 0;
                    //set the hostname data
                    for (unsigned x = (i+strlen(toSearch)); x < strlen(buf);x++)
                    {
                        returnHost[count+5]=buf[x];
                        count++;
                    }
 
                    //send the pong
                    if (sendData(returnHost, s))
                    {
                        cout << timeNow() <<"  Ping Pong" << endl;
                    }
 
                    return;
                }
            }
        }
 
}

char * timeNow()
{//returns the current date and time
    time_t rawtime;
    struct tm * timeinfo;
 
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
 
    return asctime (timeinfo);
}*/