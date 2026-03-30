#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static std::string trim(const std::string &s){
    size_t a=0,b=s.size();
    while(a<b && isspace((unsigned char)s[a])) a++;
    while(b>a && isspace((unsigned char)s[b-1])) b--;
    return s.substr(a,b-a);
}

int main(){
    const int port = 21465;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s<0) return 1;
    sockaddr_in a; std::memset(&a,0,sizeof(a));
    a.sin_family = AF_INET; a.sin_addr.s_addr = inet_addr("127.0.0.1"); a.sin_port = htons(port);
    if(bind(s,(sockaddr*)&a,sizeof(a))<0) return 1;
    std::cout<<"Authentication Server is up and running using UDP on port "<<port<<"."<<std::endl;
    char buf[2048];
    const std::string fname = "users.txt";
    while(true){
        sockaddr_in peer; socklen_t plen = sizeof(peer);
        ssize_t r = recvfrom(s, buf, sizeof(buf)-1, 0, (sockaddr*)&peer, &plen);
        if(r<=0) continue;
        buf[r]=0;
        std::string msg(buf);
        std::istringstream ss(msg);
        std::string cmd; ss>>cmd;
        if(cmd=="AUTH_CHECK"){
            std::string user,h; ss>>user>>h;
            std::string suffix = h.size()>=5 ? h.substr(h.size()-5) : h;
            std::cout<<"Authentication Server has received an authentication request for a user with hash suffix: "<<suffix<<"."<<std::endl;
            bool ok=false;
            std::ifstream fi(fname);
            std::string line;
            while(std::getline(fi,line)){
                if(line.find(h)!=std::string::npos){ ok=true; break; }
            }
            if(ok){
                std::cout<<"Authentication succeeded for a user with hash suffix: "<<suffix<<"."<<std::endl;
                std::string rep="AUTH_RESULT SUCCESS\n";
                sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
            } else {
                std::cout<<"Authentication failed for a user with hash suffix: "<<suffix<<"."<<std::endl;
                std::string rep="AUTH_RESULT FAIL\n";
                sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
            }
        } else if(cmd=="SHUTDOWN"){
            break;
        } else {
            std::string rep="UNKNOWN_CMD\n";
            sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
        }
        std::cout << "The Authentication Server has sent the authentication result to the Hospital Server." << std::endl;
    }
    close(s);
    return 0;
}