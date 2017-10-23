//TODO
//IRC je case insensitive pri nazvoch kanalov - kanaly mam ulozene v ich lowercase verzii
//cim je ukonceny syslog?
//moze byt highlight aj mezera/biely znak?
//the characters {}| are
//   considered to be the lower case equivalents of the characters []\
//odchytit SIGINT a ukoncit predtym spojenie so serverom
//TODO pozriet pri ktorych spravach zacinajucich 4/5 treba ukoncit bota
//co znamena: funkce logovat ? - ano funkce logovat -- jaaj, ?today a ?msg su funkcie

#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdarg.h>
#include <vector>
#include <regex>
#include <iterator>
#include <signal.h>

using namespace std;

enum tError {
    eOK = 0,
    eARG_NUMBER,
    eARG,
    eADDR,
    eCH,
    eSOCK,
    eDNS,
    eCONN,
    eSEND,
    eRECV,
    eMESG,
    eSERVER,
    eUNKNOWN
};

// Global Variables
// -- parsed input parameters
string address;
string port;
string channels;
string syslog;
string lights;
// -- parsed message parts
string prefix;
string command;
vector<string> parameters;
string trail;
// -- connection to IRC server
int sock;

const char *NICK = "potato_";
const char *eMSG[]{
        "",
        "Error: Invalid number of arguments. Rerun with -h for help.\n",
        "Error: Invalid argument. Rerun with -h for help.\n",
        "Error: Invalid hostname. Not valid IPv4 or IPv6 address.\n",
        "Error: Channels are in wrong format.",
        "Error: Can't create socket\n",
        "Error: Get host by name error.\n",
        "Error: Can't create connection.\n",
        "Error: Problem while sending message to server.\n",
        "Error: Problem while receiving message from server.\n",
        "Error: Unknown message received from server.\n",
        "Error:\n",
        "Error: Received error message from server.\n"
        "Error: Something unexpected happened.\n"
};

void my_handler(int s);
void handleError(int eCode);
void printHelp();
bool is_ipv4_address(const string& str);
bool is_ipv6_address(const string& str);
void connectTo(int *sock);
void talkTo(int *sock);
void parseLine(const string message);
vector<string> split(const string& str, const string& delim);
string toLowercase(string source);
void raw(int *s, char *fmt, ...);
string timeNow();
string todayTime();
void sendSyslog(string key, string name);

/**
 * Entry point
 * @param  argc 	number of command line arguments
 * @param  argv 	command line arguments
 * @return      	error code
 */
int main (int argc, char *argv[]) {

  signal (SIGINT,my_handler);

  // consider providing help
  //TODO
  //-h nebo --help se má chovat jako na linuxu? Tedy že pokud je kdekoliv za ./isabot tak se vypíše nápověda? - ano chování jako v linuxu.
	if (argc < 3) {
		if (argc == 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
			printHelp();
			handleError(eOK);
		} else {
      handleError(eARG_NUMBER);
		}
	}

  // number of arguments is out of allowed range
	if ((argc > 7) || argc % 2 == 0) {
    handleError(eARG_NUMBER);
	}

	if (argc >= 3) {
		address = string(argv[1]);
		channels = string(argv[2]);
		if (argc == 5) {
			if (strcmp(argv[3], "-s") == 0) {
				syslog = argv[4];
			} else if (strcmp(argv[3], "-l") == 0) {
				lights = argv[4];
			} else {
				handleError(eARG);
			}
		} else if (argc == 7) {
			if ((strcmp(argv[3], "-s") == 0) && (strcmp(argv[5], "-l") == 0)) {
				syslog = argv[4];
				lights = argv[6];
			} else if ((strcmp(argv[3], "-l") == 0) && (strcmp(argv[5], "-s") == 0)) {
				lights = argv[4];
				syslog = argv[6];
			} else {
				handleError(eARG);
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

  // control format of channels
  regex channel("(#|&)[^\x07\x2C\x20]{0,199}");
  for (string ch : split(channels, ",")) {
    if (!regex_match(ch, channel)) {
      handleError(eCH);
    }
  }
  //TODO
  //skontrolovat format syslog
  //ak je prazdny, nastavid default

  //TODO
  //ak nie su zadane highlights, nic nelogovat

  connectTo(&sock);
  talkTo(&sock);

  close(sock);
  return eOK;

}

void my_handler(int s) {

  raw(&sock, "QUIT :IRC bot stopped\r\n");
  printf("Caught signal %d\n",s);
  handleError(eOK);

}

/**
 * Prints error code message end ends program
 * @param eCode error code
 */
void handleError(int eCode) {

  if (eCode < eOK || eCode > eUNKNOWN) {
    eCode = eUNKNOWN;
  }

  cerr << eMSG[eCode] << endl;
  if(sock) {
    close(sock);
  }
  exit(eCode);
}

/**
* Prints help
*/
void printHelp() {
  cout << "------------------------------------------------------------------------------------\n";
  cout << "isabot HOST[:PORT] CHANNELS [-s SYSLOG_SERVER] [-l HIGHLIGHT] [-h|--help]\n";
	cout << "  HOST je název serveru (např. irc.freenode.net)\n";
	cout << "  PORT je číslo portu, na kterém server naslouchá (výchozí 6667)\n";
	cout << "  CHANNELS obsahuje název jednoho či více kanálů, na které se klient připojí (název kanálu je zadán včetně úvodního # nebo &; v případě více kanálů jsou tyto odděleny čárkou)\n";
  cout << "  -s SYSLOG_SERVER je ip adresa logovacího (SYSLOG) serveru\n";
  cout << "  -l HIGHLIGHT seznam klíčových slov oddělených čárkou (např. \"ip,tcp,udp,isa\")\n";
  cout << "example:\n";
  cout << "  isabot irc.freenode.net:6667 \"#ISAChannel,#IRC\" -s 192.168.0.1 -l \"ip,isa\"\n";
  cout << "------------------------------------------------------------------------------------\n";
}

/**
 * Check if given address is valid IPv4 address
 * @param  str string containing address
 * @return     true, if str is valid IPv4 address, false otherwise
 */
bool is_ipv4_address(const string& str) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, str.c_str(), &(sa.sin_addr))!=0;
}

/**
 * Check if given address is valid IPv6 address
 * @param  str string containing address
 * @return     true, if str is valid IPv6 address, false otherwise
 */
bool is_ipv6_address(const string& str) {
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
void connectTo(int *sock) {

   struct addrinfo hints;
   struct addrinfo *result, *rp;
   int sfd, s;

   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;

   s = getaddrinfo(address.c_str(), port.c_str(), &hints, &result);
   if (s != 0) {
     handleError(eDNS);
   }

   for (rp = result; rp != NULL; rp = rp->ai_next) {
     sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
     if (sfd == -1) {
       continue;
    }

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    close(sfd);
  }

  if (rp == NULL) {
    handleError(eCONN);
  }

  freeaddrinfo(result);

  *sock = sfd;
}

void talkTo(int *sock) {

  char *user, *where, *message, *sep, *target;
  int i, j, l, sl, o = -1, start, wordcount;
  char sbuf[512];
  char buf[513];
  // map<user, vector<channel>>
  map<string, vector<string>> users;
  // map<user, map<channel, vector<message>>>
  map<string, map<string, vector<string>>> backlog;

  vector<string> keys = split(lights, ",");

  raw(sock, "USER %s %s %s :%s\r\n", NICK, NICK, NICK, NICK);
  raw(sock, "NICK %s\r\n", NICK);

  while ((sl = read(*sock, sbuf, 512))) {
      for (i = 0; i < sl; i++) {
          o++;
          buf[o] = sbuf[i];
          if ((i > 0 && sbuf[i] == '\n' && sbuf[i - 1] == '\r') || o == 512) {
              buf[o + 1] = '\0';
              l = o;
              o = -1;

              printf(">> %s", buf);
              parseLine(string(buf));

              if(command.compare("001") == 0) {
                  raw(sock, "JOIN %s\r\n", channels.c_str());

              // RPL_NAMREPLY - info about users on channels
              } else if (command.compare("353") == 0) {
                // get users
                vector<string> trail_users = split(trail, " ");
                // get channel for those users + lower it for case insensitive search
                string chan = toLowercase(parameters[2]);
                for (string user : trail_users) {
                  if (user[0] == '@' || user[0] == '+') {
                    user = user.substr(1);
                  }
                  // user is not stored - create new entry
                  if (users.find(user) == users.end()) {
                    vector<string> vector_chan;
                    vector_chan.push_back(chan);
                    users.insert(pair<string, vector<string>>(user, vector_chan));
                  // user is already stored - add channel to his channels
                  } else {
                    users[user].push_back(chan);
                  }
                }

                //print stored users in channels
                cout << "POVODNY STAV: \n";
                for (pair<string, vector<string>> room : users) {
                  cout << room.first << endl;
                  for (string person: room.second) {
                    cout << person << ",";
                  }
                  cout << endl;
                }

              } else if (command[0] == '4' || command[0] == '5') {
                raw(&sock, "QUIT :IRC bot ending\r\n");
                handleError(eSERVER);
              }

              } else if (command.compare("PING") == 0) {
                buf[1] = 'O';
                raw(sock, buf);
              } else if (command.compare("NOTICE") == 0 || command.compare("PRIVMSG") == 0) {
                  // see, if we need to log this message
                  vector<string> words = split(trail, " ");
                  for(string word : words) {
                    for(string key: keys) {
                      if (word.compare(key) == 0) {
                        // log only messages with nickname
                        if (!prefix.empty()) {
                          sendSyslog(key, prefix.substr(0, prefix.find("!")));
                        }
                      }
                    }
                  }

                  if (command.compare("PRIVMSG") == 0) {
                    //TODO
                    // look for ?today and ?msg
                    // ?today odosle cas stroja, kde bezi bot

                    //sprava musi obsahovat iba toto slovo, inak sa to nepovazuje za spravne volanie funkcie
                    if (trail.compare("?today") == 0) {
                      vector<string> target_chan;
                      // get all target channels
                      for (string ch : split(parameters[0], ",")) {
                        if (ch[0] == '#' || ch[0] == '&') {
                          target_chan.push_back(ch);
                        }
                      }
                      for(string ch : target_chan) {
                        raw(sock, "PRIVMSG %s :%s\r\n", ch.c_str(), todayTime().c_str());
                      }
                    } else if (strncmp(trail.c_str(), "?msg", 4) == 0) {
                      string chan = parameters[0];
                      if ((chan[0] == '#' || chan[0] == '&') && chan.find(',') == string::npos) {
                        string second = split(trail, " ")[1];
                        int pos = second.find(':');
                        if (pos != string::npos) {
                          string name = second.substr(0, pos);
                          string msg = second.substr(pos + 1);

                          bool send = false;
                          if (users.find(name) != users.end()) {
                            vector<string> vector_chan = users.find(name)->second;
                            if (find(vector_chan.begin(), vector_chan.end(), toLowercase(chan)) != vector_chan.end()) {
                              raw(sock, "PRIVMSG %s :%s:%s\r\n", chan.c_str(), name.c_str(), msg.c_str());
                              send = true;
                            }
                          }

                          if(!send) {
                            // there is no message stored for this user
                            if (backlog.find(name) == backlog.end()) {
                              vector<string> vector_msg;
                              vector_msg.push_back(msg);
                              map<string, vector<string>> map_chan;
                              map_chan.insert(pair<string, vector<string>>(toLowercase(chan), vector_msg));
                              backlog.insert(pair<string, map<string, vector<string>>>(name, map_chan));
                            } else {
                              // there is message for this user, but not for this channel
                              if(backlog[name].find(toLowercase(chan)) == backlog[name].end()) {
                                vector<string> vector_msg;
                                vector_msg.push_back(msg);
                                backlog[name].insert(pair<string, vector<string>>(toLowercase(chan), vector_msg));
                              // there is message for this user on this channel
                              } else {
                                backlog[name][toLowercase(chan)].push_back(msg);
                              }
                            }
                          }
                        }
                      }
                    }
                  }

              } else if (command.compare("JOIN") == 0) {
                string user = prefix.substr(0, prefix.find("!"));

                // to prevent double storing my channels
                if(user.comapre(NICK) != 0) {
                  vector<string> vector_chan = split(toLowercase(parameters[0]), ",");

                  if (users.find(user) == users.end()) {
                    users.insert(pair<string, vector<string>>(user, vector_chan));
                  } else {
                    for (string chan : vector_chan) {
                      users[user].push_back(chan);
                    }
                  }

                  //print stored users in channels
                  cout << "PO JOIN STAV: \n";
                  for (pair<string, vector<string>> room : users) {
                    cout << room.first << endl;
                    for (string person: room.second) {
                      cout << person << ",";
                    }
                    cout << endl;
                  }

                  if (backlog.find(user) != backlog.end()) {
                    for (string ch : vector_chan) {
                      if(backlog[user].find(ch) != backlog[user].end()) {
                        // send all messages for user on channel
                        for (string m : backlog[user][ch]) {
                          raw(sock, "PRIVMSG %s :%s:%s\r\n", ch.c_str(), user.c_str(), m.c_str());
                        }
                        // delete send messages
                        backlog[user].erase(ch);
                        if (backlog.find(user)->second.size() == 0) {
                          backlog.erase(user);
                        }
                      }
                    }
                  }
                }
              } else if (command.compare("PART") == 0) {
                vector<string> vector_chan = split(toLowercase(parameters[0]), ",");
                string user = prefix.substr(0, prefix.find("!"));

                for (string chan : vector_chan) {
                  users.find(user)->second.erase(remove(users.find(user)->second.begin(), users.find(user)->second.end(), chan), users.find(user)->second.end());
                  if (users.find(user)->second.size() == 0) {
                    users.erase(user);
                  }
                }

                //print stored users in channels
                cout << "PO PART STAV: \n";
                for (pair<string, vector<string>> room : users) {
                  cout << room.first << endl;
                  for (string person: room.second) {
                    cout << person << ",";
                  }
                  cout << endl;
                }
              } else if (command.compare("KICK") == 0) {
                vector<string> vector_chan = split(toLowercase(parameters[0]), ",");
                vector<string> vector_user = split(parameters[1], ",");

                //TODO osetrovat, ci ten user a channel je v mojom zozname?
                for (string user: vector_user) {
                  for (string chan : vector_chan) {
                    users.find(user)->second.erase(remove(users.find(user)->second.begin(), users.find(user)->second.end(), chan), users.find(user)->second.end());
                    if (users.find(user)->second.size() == 0) {
                      users.erase(user);
                    }
                  }
                }
              }  else if (command.compare("QUIT") == 0) {
                users.erase(parameters[0]);
              } else if (command.compare("NICK") == 0) {
                string oldName = prefix.substr(0, prefix.find("!"));
                string user = trail;

                vector<string> old = users[oldName];
                users.insert(pair<string, vector<string>>(user, old));
                users.erase(oldName);

                //print stored users in channels
                cout << "PO NICK STAV: \n";
                for (pair<string, vector<string>> room : users) {
                  cout << room.first << endl;
                  for (string person: room.second) {
                    cout << person << ",";
                  }
                  cout << endl;
                }

                //TODO skontrolovat, ci nemam pre tohto usera nejaku spravu
                vector<string> vector_chan = users[user];
                if (backlog.find(user) != backlog.end()) {
                  for (string ch : vector_chan) {
                    if(backlog[user].find(ch) != backlog[user].end()) {
                      // send all messages for user on channel
                      for (string m : backlog[user][ch]) {
                        raw(sock, "PRIVMSG %s :%s:%s\r\n", ch.c_str(), user.c_str(), m.c_str());
                      }
                      // delete send messages
                      backlog[user].erase(ch);
                      if (backlog.find(user)->second.size() == 0) {
                        backlog.erase(user);
                      }
                    }
                  }
                }
              }
          }
      }
  }
}

string toLowercase(string source) {
  string outcome;
  for (char c : source) {
    outcome.push_back((char)tolower(c));
  }
  return outcome;
}

void raw(int *s, char *fmt, ...) {
    char sbuf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sbuf, 512, fmt, ap);
    va_end(ap);
    printf("<< %s", sbuf);
    write(*s, sbuf, strlen(sbuf));
}

void parseLine(const string msg)
  {
      //if (message.empty())
      //{
        //TODO
          // Garbage in, garbage out
        //  return IrcMessage();
      //}

      // https://tools.ietf.org/html/rfc1459	-- Original specification
      // https://tools.ietf.org/html/rfc2810	-- Architecture specfication
      // https://tools.ietf.org/html/rfc2811	-- Channel specification
      // https://tools.ietf.org/html/rfc2812	-- Client specification
      // https://tools.ietf.org/html/rfc2813	-- Server specification
      //
      // <message>  ::= [':' <prefix> <SPACE> ] <command> <params> <crlf>
      // <prefix>   ::= <servername> | <nick> [ '!' <user> ] [ '@' <host> ]
      // <command>  ::= <letter> { <letter> } | <number> <number> <number>
      // <SPACE>    ::= ' ' { ' ' }
      // <params>   ::= <SPACE> [ ':' <trailing> | <middle> <params> ]
      //
      // <middle>   ::= <Any *non-empty* sequence of octets not including SPACE
      //                or NUL or CR or LF, the first of which may not be ':'>
      // <trailing> ::= <Any, possibly *empty*, sequence of octets not including
      //                  NUL or CR or LF>
      //
      // <crlf>     ::= CR LF

      // reset global variables before parsing
      prefix = "";
      command = "";
      parameters.clear();
      trail = "";
      //remove \r\n
      string message = msg.substr(0, msg.length() - 2);

      // Parameters are between command and trail
      auto trailDivider = message.find(" :");
      bool haveTrailDivider = trailDivider != message.npos;

      // Assemble outputs
      vector<string> parts;

      // With or without trail
      if (haveTrailDivider)
      {
          // Have trail, split by trail
          string uptotrail = message.substr(0, trailDivider);
          trail = message.substr(trailDivider + 2);
          parts = split(uptotrail, " ");
      }
      else
      {
          // No trail, everything are parameters
          parts = split(message, " ");
      }

      enum class DecoderState
      {
          PREFIX,
          COMMAND,
          PARAMETER
      } state;

      bool first = true;
      state = DecoderState::PREFIX;

      for (const string part : parts)
      {

          switch (state)
          {
              // Prefix, or command... have to be decided
              case DecoderState::PREFIX:
              case DecoderState::COMMAND:
              {
                  // Prefix, aka origin of message
                  bool havePrefix = part[0] == ':';

                  if (havePrefix && first)
                  {
                      // Oh the sanity
                      if (part.size() < 2)
                      {
                          //TODO
                          //return IrcMessage();
                      }

                      // Have prefix
                      state = DecoderState::COMMAND;
                      prefix = part.substr(1);
                      first = false;
                  }
                  else
                  {
                      // Have command
                      state = DecoderState::PARAMETER;
                      command = part;
                  }

                  break;
              }
              case DecoderState::PARAMETER:
              {
                  parameters.push_back(part);
                  break;
              }
          }

      }


      //cout << "PREFIX: " << prefix << "\n";
      //cout << "COMMAND: " << command << "\n";
      //cout << "PARAMETERS: ";
      //for (auto const& c : parameters)
        //std::cout << c << ' ';
      //cout << "\nTRAILS: " << trail << "\n";

      // Construct an IrcMessage
      //IrcMessage ircmsg(command, prefix, std::move(parameters), trail);
      //m_Handles(ircmsg);

      //return ircmsg;
  }

//https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
  vector<string> split(const string& str, const string& delim)
  {
      vector<string> tokens;
      size_t prev = 0, pos = 0;
      do
      {
          pos = str.find(delim, prev);
          if (pos == string::npos) pos = str.length();
          string token = str.substr(prev, pos-prev);
          if (!token.empty()) tokens.push_back(token);
          prev = pos + delim.length();
      }
      while (pos < str.length() && prev < str.length());
      return tokens;
  }


string timeNow()
{//returns the current date and time
    /*time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    return asctime (timeinfo);*/
    char buff[20];
    struct tm *sTm;

    time_t now = time (0);
    sTm = gmtime (&now);

    strftime (buff, sizeof(buff), "%b %m %H:%M:%S", sTm);

    return string(buff);
    //printf ("%s %s\n", buff, "Event occurred now");

}

string todayTime()
{//returns the current date and time
    /*time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    return asctime (timeinfo);*/
    char buff[20];
    struct tm *sTm;

    time_t now = time (0);
    sTm = gmtime (&now);

    strftime (buff, sizeof(buff), "%d.%m.%Y", sTm);

    return string(buff);
    //printf ("%s %s\n", buff, "Event occurred now");

}

void sendSyslog(string key, string name) {
  cout << "______________\n||logujem||\n_____________";
  //TODO vytvarat spojenie pre kazdu spravu zvlast?
  //cim ma byt ukoncena sprava?

  const int BUFLEN = 1025;
  //TODO nezabudnut upravit port
  const int PORT = 5140;
  struct sockaddr_in si_other;
  int s, i, slen=sizeof(si_other);
  char buf[BUFLEN];

  if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
      handleError(eSOCK);
  }

  memset((char *) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(PORT);

  if (inet_aton(syslog.c_str(), &si_other.sin_addr) == 0)
  {
      handleError(eDNS);
  }

  //TODO zatial mi to vzdy vrati 127.0.0.1 - je to vpohode?
  //TODO moze tam byt <134> napevno?

  string msg = "<134>" + timeNow() + " " + inet_ntoa(si_other.sin_addr) + " " + key + " " + name + ": "+ trail;

  //send the message
  //TODO pozriet, ci strlen nebude robit problemy
  if (sendto(s, msg.c_str(), msg.length() , 0 , (struct sockaddr *) &si_other, slen)==-1)
  {
      handleError(eSEND);
  }



}
