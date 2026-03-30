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

static bool save_presc_atomic(const std::string &fname,const std::string &line){
    std::string tmp=fname+".tmp";
    std::ifstream in(fname);
    std::ofstream fo(tmp,std::ios::trunc);
    if(!fo) return false;
    std::string l;
    while(std::getline(in,l)) fo<<l<<"\n";
    fo<<line<<"\n";
    fo.close();
    if(std::rename(tmp.c_str(),fname.c_str())!=0) return false;
    return true;
}

int main(){
    const int port = 22465;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s<0) return 1;
    sockaddr_in a; std::memset(&a,0,sizeof(a));
    a.sin_family=AF_INET; a.sin_addr.s_addr=inet_addr("127.0.0.1"); a.sin_port=htons(port);
    if(bind(s,(sockaddr*)&a,sizeof(a))<0) return 1;
    std::cout<<"Prescription Server is up and running using UDP on port "<<port<<"."<<std::endl;
    char buf[4096];
    const std::string fname="prescriptions.txt";
    while(true){
        sockaddr_in peer; socklen_t plen=sizeof(peer);
        ssize_t r=recvfrom(s,buf,sizeof(buf)-1,0,(sockaddr*)&peer,&plen);
        if(r<=0) continue;
        buf[r]=0; std::string msg(buf);
        std::istringstream ss(msg);
        std::string cmd; ss>>cmd;
        if(cmd=="PRESCRIBE"){
            std::string doc,phash,med,freq;
            ss>>doc>>phash>>med>>freq;
            std::string psuf = phash.size()>=5 ? phash.substr(phash.size()-5) : phash;
            std::cout<<"Prescription Server has received a request from "<<doc<<" to prescribe the user with hash suffix: "<<psuf<<"."<<std::endl;
            std::string line = doc + " " + phash + " " + med + " " + freq;
            bool ok = save_presc_atomic(fname,line);
            if(ok){
                std::cout<<"Successfully saved the prescription details for user with hash suffix: "<<psuf<<"."<<std::endl;
                std::string rep="PRESC_OK\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
            } else {
                std::string rep="PRESC_FAIL_IO\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
            }
        } else if(cmd=="VIEW_PRESC"){
            std::string target; ss>>target;
            std::string psuf = target.size()>=5 ? target.substr(target.size()-5) : target;
            std::cout<<"The prescription server has received a request to view the prescription for the user with hash suffix: "<<psuf<<"."<<std::endl;
            std::string out;
            std::ifstream fi(fname);
            std::string l;
            bool any=false;
            while(std::getline(fi,l)){
                if(target=="ALL"){ out += l + "|"; any=true; continue; }
                std::istringstream ls(l);
                std::string d,p,med,freq;
                ls>>d>>p>>med>>freq;
                if(target==d || target==p){
                    any=true;
                    if(freq=="None"){
                        out = "NONE\n";
                    } else {
                        out += d + " " + p + " " + med + " " + freq + "|";
                    }
                }
            }
            if(!any){
                std::cout<<"There are no current prescriptions for this user."<<std::endl;
                std::string rep="NO_PRESC\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
            } else {
                if(out=="NONE\n"){
                    std::cout<<"There are no current prescriptions for this user."<<std::endl;
                    std::string rep="NO_PRESC\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
                } else {
                    std::cout<<"A prescription exists for this user."<<std::endl;
                    out += "\n";
                    sendto(s,out.c_str(),out.size(),0,(sockaddr*)&peer,plen);
                }
            }
        } else if(cmd=="SHUTDOWN"){
            break;
        } else {
            std::string rep="UNKNOWN_CMD\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
        }
    }
    close(s);
    return 0;
}