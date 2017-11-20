/*
 * ISA Projekt 2017/2018 - IRC bot s logováním SYSLOG
 * Autor: Katarína Grešová (xgreso00)
 * mail: xgreso00@stud.fit.vutbr.cz
 */

// used libraries
#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdarg.h>
#include <regex>
#include <signal.h>
#include <stdio.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_link.h>
#include <ifaddrs.h>

using namespace std;

// constants
const char* NICK = "xgreso00";
const char* IRC_DEFAULT_PORT = "6667";
const char* SYSLOG_SERVER = "127.0.0.1";
const uint16_t SYSLOG_PORT = 514;
const char* SYSLOG_PRI_PART = "<134>";
const char* SYSLOG_MY_NAME = "isabot";
const long MIN_PORT = 1;
const long MAX_PORT = 65535;
const char* eMSG[] {
  "",
  "Error: Invalid number of arguments. Rerun with -h for help.\n",
  "Error: Invalid port number.\n",
  "Error: Invalid channels format.\n",
  "Error: Invalid argument. Rerun with -h for help.\n",
  "Error: Invalid syslog server IPv4 address.\n",
  "Error: Get host by name error.\n",
  "Error: Can't create connection to server.\n",
  "Error: Received error message from IRC server.\n",
  "Error: Message received from IRC server is in invalid format.\n",
  "Error: Can't create socket\n",
  "Error: Problem while sending message to server.\n",
  "Error: Problem while finding IP address for this device.\n"
  "Error: Something unexpected happened.\n"
};

// global variables
// -- connection to IRC server
int sock;

enum tError {
  eOK = 0,
  eARG_NUMBER,
  ePORT,
  eCH,
  eARG,
  eSYS_SERV,
  eDNS,
  eCONN,
  eSERVER,
  eMESG,
  eSOCK,
  eSEND,
  eMYIP,
  eUNKNOWN
};

enum DecoderState {
    PREFIX,
    COMMAND,
    PARAMETER
};

struct ParsedInput {
  string address;
  string port;
  string channels;
  string syslog;
  vector<string> lights;
};

struct ParsedMsg {
  string prefix;
  string command;
  vector<string> parameters;
  string trail;
};

// function declarations
void myHandler(int s);
void handleError(int eCode);
tError parseInput(int argc, char* argv[], ParsedInput* input);
void printHelp();
bool isValidPort(const char* port);
vector<string> split(const string& str, const string& delim);
bool isIpv4Address(const string& str);
tError connectTo(const char* address, const char* port);
tError talkTo(ParsedInput* input);
void sendMsg(const char *fmt, ...);
tError parseLine(string buf, ParsedMsg* msg);
void fillUsers(map<string, vector<string>>* users, ParsedMsg* msg);
string timeNow(const char* format);
tError sendSyslog(string name, string syslog_server, string trail);
void handleTodayFunction(string channels);
void handleMsgFunction(ParsedMsg* msg, map<string, vector<string>>* users, map<string, map<string, vector<string>>>* backlog);
void handleJoin(ParsedMsg* msg, map<string, vector<string>>* users, map<string, map<string, vector<string>>>* backlog);
void handleNick(ParsedMsg* msg, map<string, vector<string>>* users, map<string, map<string, vector<string>>>* backlog);
void sendBacklog( map<string, map<string, vector<string>>>* backlog, vector<string> vector_chan, string user);
string toLowercase(string source);
string getMyIP();

/**
 * Entry point
 * @param  argc 	number of command line arguments
 * @param  argv 	command line arguments
 * @return      	error code
 */
int main (int argc, char* argv[]) {

  // handling end of program with SIGINT
  signal(SIGINT, myHandler);

  // structure for storing parsed input arguments
  ParsedInput input;

  // parse input arguments
  tError eCode = parseInput(argc, argv, &input);
  if (eCode != eOK) {
    handleError(eCode);
  }

  // create connection with IRC server
  eCode = connectTo(input.address.c_str(), input.port.c_str());
  if (eCode != eOK) {
    handleError(eCode);
  }

  eCode = talkTo(&input);
  if (eCode != eOK) {
    handleError(eCode);
  }

  close(sock);
  return eOK;

}

/**
 * Handles interupts
 * @param  s    received signal
 */
void myHandler(int s) {
  if (sock) {
    sendMsg("QUIT :IRC bot stopped\r\n");
  }
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

  cerr << eMSG[eCode];

  if (sock) {
    close(sock);
  }

  exit(eCode);
}

/**
 * Parses input argument and stores them in structure
 * @param  argc   number of input arguments
 * @param  argv   array with input arguments
 * @param  input  structure for storing parsed input arguments
 * @return        error code
 */
tError parseInput(int argc, char* argv[], ParsedInput* input) {

  // check, if help is requested
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printHelp();
      handleError(eOK);
    }
  }

  // number of arguments is out of allowed range
	if (argc > 7 || argc < 3 || (argc % 2 == 0)) {
    return eARG_NUMBER;
	}

  // first argument have to be HOST[:PORT]
  string address = string(argv[1]);
  unsigned index;
  if ((index = address.find(":")) != (unsigned) string::npos) {
    string port = address.substr(index + 1);
    if (isValidPort(port.c_str())) {
      input->port = string(port);
      input->address = address.substr(0, index);
    } else {
      return ePORT;
    }
  } else {
    input->address = address;
  	input->port = IRC_DEFAULT_PORT;
  }

  // second argument have to be CHANNELS
  string channels = argv[2];
  regex channel("(#|&)[^\x07\x2C\x20]{0,199}");
  for (string ch : split(channels, ",")) {
    if (!regex_match(ch, channel)) {
      handleError(eCH);
    }
  }
  input->channels = channels;

  // looking for optional arguments
  string syslog;
  string lights;
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

  // validate optional arguments
  // no highlights specified -> will not log -> my job in this function is done
  if (lights.empty()) {
    return eOK;
  } else {
    input->lights = split(lights, ",");
    if (syslog.empty()) {
      input->syslog = SYSLOG_SERVER;
    } else {
      if (!isIpv4Address(syslog)) {
        return eSYS_SERV;
      } else {
        input->syslog = syslog;
      }
    }
  }

  return eOK;
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
 * Checks if value is valid port number
 * @param  port   value to validate
 * @return        true, if value is valid port number, false otherwise
 */
bool isValidPort(const char* port) {
  char *end;
  long longPort = strtol(port, &end, 10);
  if (longPort > MAX_PORT || longPort < MIN_PORT || *end != '\0'){
    return false;
  }
  return true;
}

/**
 * Splits string using string dlimiter
 * @param  str    string to split
 * @param  delim  delimiter
 * @return        splitted parts
 *
 * This function was found on https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
 */
vector<string> split(const string& str, const string& delim) {
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

/**
 * Check if given address is valid IPv4 address
 * @param  str string containing address
 * @return     true, if str is valid IPv4 address, false otherwise
 */
bool isIpv4Address(const string& str) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, str.c_str(), &(sa.sin_addr)) != 0;
}

/**
 * Connects to given IPv4/IPv6 address and return socket in parameter
 * @param  addr IPv4/IPv6 address to connect to
 * @param  sock connected socket is returned here
 * @return      error code
 *
 * Function contains code from getaddinfo(3) man page
 */
tError connectTo(const char* address, const char* port) {

  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  s = getaddrinfo(address, port, &hints, &result);
  if (s != 0) {
    return eDNS;
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
    return eCONN;
  }

  freeaddrinfo(result);
  sock = sfd;
  return eOK;
}

/**
 * Handles communication with IRC server
 * @param  input    parsed inpur arguments
 * @return          error code
 * Main loop of this function is from https://gist.github.com/codeniko/c3f0c7d1399bac66d821
 */
tError talkTo(ParsedInput* input) {

  int i, sl, o = -1;
  char sbuf[512];
  char buf[513];
  map<string, vector<string>> users;
  map<string, map<string, vector<string>>> backlog;

  sendMsg("NICK %s\r\n", NICK);
  sendMsg("USER %s %s %s :%s\r\n", NICK, NICK, NICK, NICK);

  while ((sl = read(sock, sbuf, 512))) {
    for (i = 0; i < sl; i++) {
      o++;
      buf[o] = sbuf[i];
      if ((i > 0 && sbuf[i] == '\n' && sbuf[i - 1] == '\r') || o == 512) {
        buf[o + 1] = '\0';
        o = -1;

        // structure for storing parsed message from IRC server
        ParsedMsg msg;
        tError eCode = parseLine(string(buf), &msg);
        if (eCode != eOK) {
          return eCode;
        }

        // time to join channels
        if(msg.command.compare("001") == 0) {
          sendMsg("JOIN %s\r\n", input->channels.c_str());

        // RPL_NAMREPLY - info about users on channels
        } else if (msg.command.compare("353") == 0) {
          fillUsers(&users, &msg);

        // some error message from IRC server
        } else if (msg.command[0] == '4' || msg.command[0] == '5') {
          sendMsg("QUIT :IRC bot ending\r\n");
          return eSERVER;

        // testing active presence
        } else if (msg.command.compare("PING") == 0) {
          buf[1] = 'O';
          sendMsg(buf);

        // messages for users - some logic is the same
        } else if (msg.command.compare("NOTICE") == 0 || msg.command.compare("PRIVMSG") == 0) {
          bool send = false;
          // see, if we need to log this message - highlight are present and match message
          if (input->lights.size() != 0) {
            vector<string> words = split(msg.trail, " ");
            for(string word : words) {
              for(string light: input->lights) {
                if (word.compare(light) == 0) {
                  // log only messages with nickname
                  if (!msg.prefix.empty()) {
                    eCode = sendSyslog(msg.prefix.substr(0, msg.prefix.find("!")), input->syslog, msg.trail);
                    if (eCode != eOK) {
                      return eCode;
                    }
                    send = true;
                    break;
                  }
                }
              }
              if (send) {
                break;
              }
            }
          }

          // there can be ERROR message encapsulated inside NOTICE message
          if (msg.command.compare("NOTICE") == 0) {
            if (strncmp(msg.trail.c_str(), "ERROR", 5) == 0) {
              sendMsg("QUIT :IRC bot ending\r\n");
              return eSERVER;
            }
          }

          // PRIVMSG might contains supported functions
          if (msg.command.compare("PRIVMSG") == 0) {

            // ?today function was used
            if (msg.trail.compare("?today") == 0) {
              handleTodayFunction(msg.parameters[0]);

            // ?msg function was used
            } else if (strncmp(msg.trail.c_str(), "?msg", 4) == 0) {
              handleMsgFunction(&msg, &users, &backlog);

            }
          }

        // new user joined channel
        } else if (msg.command.compare("JOIN") == 0) {
            handleJoin(&msg, &users, &backlog);

        // user left channel
        } else if (msg.command.compare("PART") == 0) {
          vector<string> vector_chan = split(toLowercase(msg.parameters[0]), ",");
          string user = msg.prefix.substr(0, msg.prefix.find("!"));

          for (string chan : vector_chan) {
            users.find(user)->second.erase(remove(users.find(user)->second.begin(), users.find(user)->second.end(), chan), users.find(user)->second.end());
            if (users.find(user)->second.size() == 0) {
              users.erase(user);
            }
          }

        // user was kicked out
        } else if (msg.command.compare("KICK") == 0) {
          vector<string> vector_chan = split(toLowercase(msg.parameters[0]), ",");
          vector<string> vector_user = split(msg.parameters[1], ",");

          for (string user: vector_user) {
            for (string chan : vector_chan) {
              users.find(user)->second.erase(remove(users.find(user)->second.begin(), users.find(user)->second.end(), chan), users.find(user)->second.end());
              if (users.find(user)->second.size() == 0) {
                users.erase(user);
              }
            }
          }

        // user completely ended connection OR user was killed
      } else if (msg.command.compare("QUIT") == 0 || msg.command.compare("KILL") == 0) {
          string user = msg.prefix.substr(0, msg.prefix.find("!"));
          // QUIT message was for me
          if (user.compare(NICK) == 0) {
            return eSERVER;
          } else {
            users.erase(user);
          }

        // user changed nickname
        } else if (msg.command.compare("NICK") == 0) {
          handleNick(&msg, &users, &backlog);

        }
      }
    }
  }
  return eOK;
}

/**
 * Sends message to IRC server
 * @param ftm   format string
 * @param ...   variables used in format string
 *
 * This function was found on https://gist.github.com/codeniko/c3f0c7d1399bac66d821
 */
void sendMsg(const char *fmt, ...) {
  char sbuf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(sbuf, 512, fmt, ap);
  va_end(ap);
  write(sock, sbuf, strlen(sbuf));
}

/**
 * Parses message from IRC server into multiple parts
 * @param  buf    message from IRC server
 * @param  msg    structure for storing message parts
 * @return        error code
 *
 * This is modified version of function from https://studiofreya.com/2015/06/27/a-simple-modern-irc-client-library-in-cpp-part-1-the-parser/
 */
tError parseLine(string buf, ParsedMsg* msg) {

  if (buf.empty()) {
    return eMESG;
  }

  // remove \r\n
  string message = buf.substr(0, buf.length() - 2);

  // Parameters are between command and trail
  auto trailDivider = message.find(" :");
  bool haveTrailDivider = trailDivider != message.npos;

  // Assemble outputs
  vector<string> parts;

  // With or without trail
  if (haveTrailDivider) {
    // Have trail, split by trail
    string uptotrail = message.substr(0, trailDivider);
    msg->trail = message.substr(trailDivider + 2);
    parts = split(uptotrail, " ");
  } else {
    // No trail, everything are parameters
    parts = split(message, " ");
  }

  DecoderState state = PREFIX;
  bool first = true;

  for (const string part : parts) {
    switch (state) {
      // Prefix, or command... have to be decided
      case PREFIX:
      case COMMAND: {
        // Prefix, aka origin of message
        bool havePrefix = part[0] == ':';

        if (havePrefix && first) {
          // Oh the sanity
          if (part.size() < 2) {
              return eMESG;
          }

          // Have prefix
          state = COMMAND;
          msg->prefix = part.substr(1);
          first = false;
        } else {
          // Have command
          state = PARAMETER;
          msg->command = part;
        }
        break;
      }
      case PARAMETER: {
        msg->parameters.push_back(part);
        break;
      }
    }
  }
  return eOK;
}

/**
 * Inserts information about users and channels they are on into structure
 * @param users   structure for storing users and channels
 * @param msg     message from IRC server with code 353
 */
void fillUsers(map<string, vector<string>>* users, ParsedMsg* msg) {
  //get channel - storing in lowercase for case insensitive comparing
  string chan = toLowercase(msg->parameters[2]);

  //get users
  vector<string> trail_users = split(msg->trail, " ");

  for(string user : trail_users) {
    // remove optional prefix
    if (user[0] == '@' || user[0] == '+') {
      user = user.substr(1);
    }

    // user is not stored - create new entry
    if (users->find(user) == users->end()) {
      vector<string> vector_chan;
      vector_chan.push_back(chan);
      users->insert(pair<string, vector<string>>(user, vector_chan));
    // user is already stored - add channel to his channels
    } else {
      (*users)[user].push_back(chan);
    }
  }
}

/**
 * Returns current date and time in given format
 * @param  format   required format
 * @return          current date and time in given format
 */
string timeNow(const char* format) {
    char buff[20];
    struct tm *sTm;

    time_t now = time (0);
    sTm = gmtime (&now);

    strftime (buff, sizeof(buff), format, sTm);

    return string(buff);
}

/**
 * Sends syslog message
 * @param name            nickname of user, who send matched message
 * @param syslog_server   address of syslog server
 * @param trail           message to log
 * @return                erro code
 */
tError sendSyslog(string name, string syslog_server, string trail) {

  struct sockaddr_in si_other;
  int s, slen = sizeof(si_other);

  if ( (s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
      return eSOCK;
  }

  memset((char *) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(SYSLOG_PORT);

  if (inet_aton(syslog_server.c_str(), &si_other.sin_addr) == 0) {
      return eDNS;
  }

  string myIP = getMyIP();
  if (myIP.empty()) {
    return eMYIP;
  }

  string msg = string(SYSLOG_PRI_PART) + timeNow("%b %m %H:%M:%S") + " " + myIP + " " + SYSLOG_MY_NAME + " " + name + ":" + trail;

  //send the message
  if (sendto(s, msg.c_str(), msg.length() , 0 , (struct sockaddr *) &si_other, slen) == -1) {
      return eSEND;
  }

  return eOK;
}

/**
 * Sends current date to specified channels
 * @param  channels   target channels
 */
void handleTodayFunction(string channels) {
  vector<string> target_chan;
  // get all target channels
  for (string ch : split(channels, ",")) {
    if (ch[0] == '#' || ch[0] == '&') {
      target_chan.push_back(ch);
    }
  }
  // send current date on all channels
  for(string ch : target_chan) {
    sendMsg("PRIVMSG %s :%s\r\n", ch.c_str(), timeNow("%d.%m.%Y").c_str());
  }
}

/** Sends message to channel if user is there, store message to backlog otherwise
 *
 * @param  msg      parsed message from IRC server
 * @param  users    users on channels
 * @param  backlog  all stored messages
 */
void handleMsgFunction(ParsedMsg* msg, map<string, vector<string>>* users, map<string, map<string, vector<string>>>* backlog) {
  // get channel
  string chan = msg->parameters[0];
  // verify, there is only one channel
  if ((chan[0] == '#' || chan[0] == '&') && chan.find(',') == string::npos) {

    // get real message - part after ?msg
    unsigned pos = (unsigned)msg->trail.find(' ');
    string second = msg->trail.substr(pos + 1);

    pos = (unsigned)second.find(':');
    // message have to be in format nickname:message
    if (pos != string::npos) {
      string name = second.substr(0, pos);
      string m = second.substr(pos + 1);

      // empty message is not valid -> do not send
      if (m.length() > 0) {
        bool send = false;
        if (users->find(name) != users->end()) {
          vector<string> vector_chan = users->find(name)->second;

          // user is online on channel - we can send message
          if (find(vector_chan.begin(), vector_chan.end(), toLowercase(chan)) != vector_chan.end()) {
            sendMsg("PRIVMSG %s :%s:%s\r\n", chan.c_str(), name.c_str(), m.c_str());
            send = true;
          }
        }

        // user is not online on channel - we will store message
        if(!send) {
          // there are no messages stored for this user
          if (backlog->find(name) == backlog->end()) {
            vector<string> vector_msg;
            vector_msg.push_back(m);
            map<string, vector<string>> map_chan;
            map_chan.insert(pair<string, vector<string>>(toLowercase(chan), vector_msg));
            backlog->insert(pair<string, map<string, vector<string>>>(name, map_chan));
          } else {
            // there is message for this user, but not for this channel
            if((*backlog)[name].find(toLowercase(chan)) == (*backlog)[name].end()) {
              vector<string> vector_msg;
              vector_msg.push_back(m);
              (*backlog)[name].insert(pair<string, vector<string>>(toLowercase(chan), vector_msg));
            // there is message for this user on this channel
            } else {
              (*backlog)[name][toLowercase(chan)].push_back(m);
            }
          }
        }
      }
    }
  }
}

/**
 * Adds new user and his channels to users, sends stored messages if necessary
 * @param  msg      parsed message from IRC server
 * @param  users    users on channels
 * @param  backlog  all stored messages
 */
void handleJoin(ParsedMsg* msg, map<string, vector<string>>* users, map<string, map<string, vector<string>>>* backlog) {
  // get user
  string user = msg->prefix.substr(0, msg->prefix.find("!"));
  // to prevent double storing my channels
  if(user.compare(NICK) != 0) {
    vector<string> vector_chan = split(toLowercase(msg->parameters[0]), ",");

    // this user is not stored yet - create new entry
    if (users->find(user) == users->end()) {
      users->insert(pair<string, vector<string>>(user, vector_chan));
    // this user is stored - append new channels
    } else {
      for (string chan : vector_chan) {
        (*users)[user].push_back(chan);
      }
    }

    // check, if we have stored some messages for this user
    if (backlog->find(user) != backlog->end()) {
      sendBacklog(backlog, vector_chan, user);
    }
  }
}

/**
 * Rename user name is users
 * @param  msg      parsed message from IRC server
 * @param  users    users on channels
 * @param  backlog  all stored messages
 */
void handleNick(ParsedMsg* msg, map<string, vector<string>>* users, map<string, map<string, vector<string>>>* backlog) {

  // get old name
  string oldName = msg->prefix.substr(0, msg->prefix.find("!"));
  // get new name
  string user = msg->trail;

  // store channels
  vector<string> old = (*users)[oldName];
  // insert channels under new user name
  users->insert(pair<string, vector<string>>(user, old));
  // delete old user entry
  users->erase(oldName);

  // check, if we have stored some messages for this user
  if (backlog->find(user) != backlog->end()) {
    sendBacklog(backlog, (*users)[user], user);
  }
}

/**
 * Sends stored messages to given user on given channels
 * @param  backlog      all stored messages
 * @param  vector_chan  channels, user is connected to
 * @param  user         nickname of user to send messages to
 */
void sendBacklog(map<string, map<string, vector<string>>>* backlog, vector<string> vector_chan, string user) {
  for (string ch : vector_chan) {
    if((*backlog)[user].find(ch) != (*backlog)[user].end()) {
      // send all messages for user on channel
      for (string m : (*backlog)[user][ch]) {
        sendMsg("PRIVMSG %s :%s:%s\r\n", ch.c_str(), user.c_str(), m.c_str());
      }
      // delete send messages
      (*backlog)[user].erase(ch);
      if (backlog->find(user)->second.size() == 0) {
        backlog->erase(user);
      }
    }
  }
}

/**
 * Converts string to lowercase
 * @param  source   string to convert
 * @return          string converted to lowercase
 */
string toLowercase(string source) {
  string outcome;
  for (char c : source) {
    outcome.push_back((char)tolower(c));
  }
  return outcome;
}

/**
 * Get IP address of current device
 * @return        first found IP address after loopback
 *
 * Function contains code from getifaddrs(3) man page
 */
string getMyIP() {
  struct ifaddrs *ifaddr, *ifa;
  int family, s, n;
  char host[NI_MAXHOST];

  if (getifaddrs(&ifaddr) == -1) {
    return string();
  }

  /* Walk through linked list, maintaining head pointer so we can free list later */
  for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }

    family = ifa->ifa_addr->sa_family;

    /* For an AF_INET* interface address, display the address */
    if (family == AF_INET || family == AF_INET6) {
      s = getnameinfo(ifa->ifa_addr,
                     (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                     host, NI_MAXHOST,
                     NULL, 0, NI_NUMERICHOST);

      if (s != 0) {
        return string();
      }

      if (strcmp(host, "127.0.0.1") != 0 ) {
        break;
      }
    }
  }

  freeifaddrs(ifaddr);
  return string(host);
}
