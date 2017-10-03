/**
 * Project: IPK - Project 2 (2017) - Klient pro výpočet matematických operací
 * Author: Katarina Gresova <xgreso00@stud.fit.vutbr.cz>
 **/

#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/md5.h>

using namespace std;

enum tError {
    eOK,
    eARG_NUMBER,
    eADDR,
    eSOCK,
    eDNS,
    eCONN,
    eSEND,
    eRECV,
    eUNKNOWN,
    eMESG
};

const char *eMSG[]{
        "",
        "Error: Invalid number of arguments. Rerun with -h for help.\n",
        "Error: Invalid hostname. Not valid IPv4 or IPv6 address.\n",
        "Error: Can't create socket\n",
        "Error: Get host by name error.\n",
        "Error: Can't create connection.\n",
        "Error: Problem while sending message to server.\n",
        "Error: Problem while receiving message from server.\n",
        "Error: Unknown message received from server.\n",
        "Error: Something unexpected happened.\n"
};

#define PORT "55555"
#define EPSILON 10e-6

// trim from left
inline std::string& ltrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from right
inline std::string& rtrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from left & right
inline std::string& trim(std::string& s, const char* t = " \t\n\r\f\v")
{
    return ltrim(rtrim(s, t), t);
}

inline std::string trim_copy(std::string s, const char* t = " \t\n\r\f\v")
{
    return trim(s, t);
}

void printError(int eCode);
void printHelp();
int connectTo(const char* addr, int *sock);
int talk(int *sock);
int evaluate(string problem, string* msg);
bool is_ipv4_address(const string& str);
bool is_ipv6_address(const string& str);
bool is_number(const string& s);

/**
 * Entry point
 * @param  argc 	number of command line arguments
 * @param  argv 	command line arguments
 * @return      	error code
 */
int main (int argc, char *argv[]) {

    int eCode = eOK;

    if (argc != 2) {
        eCode = eARG_NUMBER;
        printError(eCode);
        return eCode;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        printHelp();
        return eCode;
    }

    if (!is_ipv4_address(argv[1]) and !is_ipv6_address(argv[1])) {
        eCode = eADDR;
        printError(eCode);
        return eCode;
    }

    int sock;
    eCode = connectTo(argv[1], &sock);
    if (eCode != eOK) {
        printError(eCode);
        return eCode;
    }

    talk(&sock);

    return 0;

}

/**
 * Print error code message
 * @param eCode error code
 */
void printError(int eCode) {
    if (eCode < eOK || eCode > eUNKNOWN)
        eCode = eUNKNOWN;

    cerr << eMSG[eCode] << endl;
    exit(eCode);
}

/**
* Prints help
*/
void printHelp() {
    cout << "-------------------------------------\n";
    cout << "ipk-client hostname\n";
    cout << "	hostname is IPv4/IPv6 address";
    cout << "\n";
    cout << "example:\n";
    cout << "	./ipk-client 2001:db8::1\n";
    cout << "-------------------------------------\n";
}

/**
 * Connects to given IPv4/IPv6 address and return socket in parameter
 * @param  addr IPv4/IPv6 address to connect to
 * @param  sock connected socket is returned here
 * @return      error code
 */
int connectTo(const char* addr, int *sock) {

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_ADDRCONFIG;
    struct addrinfo* res = 0;
    int err = getaddrinfo(addr, PORT, &hints, &res);
    if (err != 0) {
        return eDNS;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        return eSOCK;
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
        return eCONN;
    }

    freeaddrinfo(res);

    *sock = fd;
    return 0;

}

/**
 * Does all communication with server
 * @param  socket to communicate with server
 * @return        error code
 */
int talk(int *socket) {

    int sock = *socket;

    unsigned char digest[16];
    const char* str = "xgreso00";

    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, str, strlen(str));
    MD5_Final(digest, &ctx);

    char mdString[33];
    for (int i = 0; i < 16; i++) {
        sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);
    }

    string msg = "HELLO ";
    msg.append(mdString);
    msg.append("\n");
    int n;

    n = (int) send(sock, msg.c_str(), msg.length(), 0);
    if (n < 0) {
        close(sock);
        printError(eSEND);
    }

    while(1) {

        char c[2];
        string response;
        do {
            memset(c, 0, 2);
            n = (int) recv(sock, c, 1, 0);
            if (n < 0) {
                close(sock);
                printError(eRECV);
            }
            response.append(c);
        } while(strcmp(c, "\n") != 0);

        unsigned index;
        if ((index = response.find(" ")) != string::npos) {
            string message = response.substr(0, index);
            if (strcmp(message.c_str(), "BYE") == 0) {
                string secret = response.substr(index + 1);
                cout << secret;
                close(sock);
                return 0;
            } else if(strcmp(message.c_str(), "SOLVE") == 0) {
                string problem = response.substr(index + 1);
                msg = string("");
                int eCode = evaluate(problem, &msg);
                if (eCode == eMESG) {
                    continue;
                }
                if (eCode != eOK) {
                    close(sock);
                    printError(eCode);
                }
                msg = "RESULT " + msg + "\n";
                n = (int) send(sock, msg.c_str(), msg.length(), 0);
                if (n < 0) {
                    close(sock);
                    printError(eSEND);
                }
            }
        } else {
            continue;
        }

    }
}

/**
 * Parses problem from string, evaluates it and retuns result as string
 * @param  problem string containing problem to solve
 * @param  msg     parameter for result. Result is returned as string, "ERROR" is returned when there is something wrong with problem
 * @return         error code
 */
int evaluate(string problem, string* message) {

    string msg;
    int index;
    string op1, op2;
    string operation;
    index = problem.find(" ");
    if (index != (int) string::npos) {
        op1 = trim_copy(problem.substr(0, index));
        cout << "|" << op1 << "|" << endl;
    } else {
        return eMESG;
    }
    problem.erase(0, index + 1);
    index = problem.find(" ");
    if (index != (int) string::npos) {
        operation = trim_copy(problem.substr(0, index));
        cout << "|" << operation << "|" << endl;
    } else {
        return eMESG;
    }
    problem.erase(0, index + 1);
    op2 = problem.substr(0, problem.length() - 1);
    cout << "|" << op2 << "|" << endl;

    if (!is_number(op1) || !is_number(op2)) {
        return eMESG;
    }

    int operand1;
    int operand2;
    try {
        operand1 = stoi(op1);
        operand2 = stoi(op2);
    } catch(...) {
        *message = string("ERROR");
        return eOK;
    }
    if (std::abs(operand2) <= EPSILON) {
        *message = string("ERROR");
        return eOK;
    }

    char result[50];
    unsigned num = 0;
    if (strcmp(operation.c_str(), "+") == 0) {
        num = sprintf(result, "%.2f", operand1 + operand2 + 0.00);
        msg = string(result, num);
    } else if (strcmp(operation.c_str(), "-") == 0) {
        num = sprintf(result, "%.2f", operand1 - operand2 - 0.00);
        msg = string(result, num);
    } else if (strcmp(operation.c_str(), "*") == 0) {
        num = sprintf(result, "%.2f", operand1 * operand2 * 1.00);
        msg = string(result, num);
    } else if (strcmp(operation.c_str(), "/") == 0) {
        num = sprintf(result, "%.2f", (float)(int)((operand1 / (operand2 * 1.00))*100)/100);
        msg = string(result, num);
    } else {
        msg = "ERROR";
    }

    *message = string(msg);
    return eOK;

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
 * Checks, if given string contains only digits
 * @param  s string to check
 * @return   true, if given string contains only digits, false otherwise
 */
bool is_number(const string& s) {
    return (s.find_first_not_of( "0123456789+-" ) == string::npos);
}