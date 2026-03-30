#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
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

struct Slot { std::string time; std::string patient; std::string illness; };

static std::map<std::string,std::vector<Slot>> load_schedule_preserve(const std::string &fname,std::vector<std::string> &order){
    std::map<std::string,std::vector<Slot>> m;
    order.clear();
    std::ifstream f(fname);
    std::string line;
    std::string cur;
    while(std::getline(f,line)){
        std::string t = trim(line);
        if(t.empty()){ cur.clear(); continue; }
        if(t.rfind("Dr.",0)==0 || t.rfind("Dr",0)==0){
            cur = t;
            if(m.find(cur)==m.end()){
                m[cur]=std::vector<Slot>();
                order.push_back(cur);
            }
        } else if(!cur.empty()){
            std::istringstream ss(t);
            std::string time; ss>>time;
            std::string rest; std::getline(ss,rest); rest=trim(rest);
            if(rest.empty()) m[cur].push_back({time,"",""});
            else {
                std::istringstream rs(rest);
                std::string ph; rs>>ph;
                std::string ill; std::getline(rs,ill); ill=trim(ill);
                m[cur].push_back({time,ph,ill});
            }
        }
    }
    return m;
}

static bool save_schedule_atomic_preserve(const std::string &fname,const std::map<std::string,std::vector<Slot>> &m,const std::vector<std::string> &order){
    std::string tmp = fname + ".tmp";
    std::ofstream fo(tmp,std::ios::trunc);
    if(!fo) return false;
    for(const auto &k: order){
        fo<<k<<"\n";
        auto it = m.find(k);
        if(it!=m.end()){
            for(const Slot &s: it->second){
                fo<<s.time;
                if(!s.patient.empty()){
                    fo<<" "<<s.patient;
                    if(!s.illness.empty()) fo<<" "<<s.illness;
                }
                fo<<"\n";
            }
        }
        fo<<"\n";
    }
    fo.close();
    if(std::rename(tmp.c_str(),fname.c_str())!=0) return false;
    return true;
}

int main(){
    const int port = 23465;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s<0) return 1;
    sockaddr_in a; std::memset(&a,0,sizeof(a));
    a.sin_family=AF_INET; a.sin_addr.s_addr=inet_addr("127.0.0.1"); a.sin_port=htons(port);
    if(bind(s,(sockaddr*)&a,sizeof(a))<0) return 1;
    std::cout<<"Appointment Server is up and running using UDP on port "<<port<<"."<<std::endl;
    const std::string fname="appointments.txt";
    char buf[8192];
    while(true){
        sockaddr_in peer; socklen_t plen=sizeof(peer);
        ssize_t r=recvfrom(s,buf,sizeof(buf)-1,0,(sockaddr*)&peer,&plen);
        if(r<=0) continue;
        buf[r]=0; std::string msg(buf);
        std::istringstream ss(msg);
        std::string cmd; ss>>cmd;
        if(cmd=="lookup" || cmd=="LOOKUP"){
            std::string dummy;
            std::getline(ss,dummy);
            std::cout<<"Appointment Server has received a doctor availability request."<<std::endl;
            std::vector<std::string> order;
            auto m = load_schedule_preserve(fname,order);
            std::string out;
            for(const auto &k: order){
                out += k + "|";
                for(auto &slot: m[k]){
                    out += slot.time;
                    if(!slot.patient.empty()){
                        out += " " + slot.patient;
                        if(!slot.illness.empty()) out += " " + slot.illness;
                    }
                    out += ";";
                }
                out += "#";
            }
            if(out.empty()) out="NO_DOCTORS\n";
            else out += "\n";
            sendto(s,out.c_str(),out.size(),0,(sockaddr*)&peer,plen);
        } else if(cmd=="SCHEDULE" || cmd=="schedule"){
            std::string doc,time,phash; ss>>doc>>time>>phash;
            std::string ill; std::getline(ss,ill); ill=trim(ill);
            std::string psuf = phash.size()>=5 ? phash.substr(phash.size()-5) : phash;
            std::cout<<"Appointment scheduling request received (time: "<<time<<", doctor: "<<doc<<", patient hash suffix: "<<psuf<<", illness: "<<ill<<")."<<std::endl;
            std::vector<std::string> order;
            auto m = load_schedule_preserve(fname,order);
            auto it = m.find(doc);
            if(it==m.end()){
                std::string rep = doc + " was not found in the system.\n";
                sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
                continue;
            } 
            bool conflict=false;
            for(auto &slot: it->second){
                if(slot.time==time){
                    if(!slot.patient.empty()) conflict=true;
                    else{ slot.patient=phash; slot.illness=ill; }
                    break;
                }
            }
            if(conflict){
                std::cout<<"The requested appointment time is not available."<<std::endl;
                std::string rep="SCHEDULE_FAIL_CONFLICT\n";
                sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
                continue;
            }
            bool ok = save_schedule_atomic_preserve(fname,m,order);
            if(!ok){ std::string rep="SCHEDULE_FAIL_IO\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen); continue; }
            std::vector<std::string> order2;
            auto m2 = load_schedule_preserve(fname,order2);
            bool found=false;
            auto it2 = m2.find(doc);
            if(it2!=m2.end()){
                for(auto &slot: it2->second){
                    if(slot.time==time && slot.patient==phash){ found=true; break; }
                }
            }
            if(found){
                std::cout<<"Appointment has been scheduled successfully for user "<<psuf<<" with "<<doc<<"."<<std::endl;
                std::string rep="SCHEDULE_OK\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
            } else {
                std::string rep="SCHEDULE_FAIL_IO\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
            }
        } else if(cmd=="view_appointment" || cmd=="VIEW_APPOINTMENT"|| cmd=="GET_APPT_FOR"){
            std::string phash; ss>>phash;
            std::string psuf = phash.size()>=5 ? phash.substr(phash.size()-5) : phash;
            std::cout<<"Appointment Server has received a view appointment command for the user with hash suffix "<<psuf<<"."<<std::endl;
            std::vector<std::string> order;
            auto m = load_schedule_preserve(fname,order);
            bool found=false;
            std::string out;
            for(const auto &k: order){
                for(auto &slot: m[k]){
                    if(slot.patient==phash){
                        found=true;
                        out = k + " " + slot.time;
                        if(!slot.illness.empty()) out += " " + slot.illness;
                        out += "\n";
                        break;
                    }
                }
                if(found) break;
            }
            if(found){
                std::cout<<"Returning details regarding the appointment for the user with hash suffix "<<psuf<<"."<<std::endl;
                sendto(s,out.c_str(),out.size(),0,(sockaddr*)&peer,plen);
            } else {
                std::cout<<"The user with hash suffix "<<psuf<<" has no appointment in the system."<<std::endl;
                std::string rep="NO_APPT\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
            }
        } else if(cmd=="cancel" || cmd=="CANCEL"){
            std::string phash; ss >> phash; 
            std::string psuf = phash.size()>=5 ? phash.substr(phash.size()-5) : phash;
            std::cout<<"Appointment Server has received a cancel appointment command for the user with hash suffix: "<<psuf<<"."<<std::endl;
            
            std::vector<std::string> order;
            auto m = load_schedule_preserve(fname,order);
            bool removed = false;
            std::string canceled_doc, canceled_time;
            
            for(const auto &k: order){
                for(auto &slot: m[k]){
                    if(slot.patient == phash){
                        canceled_doc = k;
                        canceled_time = slot.time;
                        slot.patient.clear();
                        slot.illness.clear();
                        removed = true;
                        break;
                    }
                }
                if(removed) break;
            }
            
            if(!removed){
                std::cout<<"Error: Failed to find appointment."<<std::endl;
                std::string rep="NO_APPT\n"; 
                sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen); 
                continue;
            }
            
            bool ok = save_schedule_atomic_preserve(fname,m,order);
            if(!ok){ 
                std::string rep="CANCEL_FAIL_IO\n"; 
                sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen); 
                continue; 
            }
            
            std::cout<<"Successfully canceled appointment."<<std::endl;
            std::string rep = canceled_doc + " " + canceled_time + "\n"; 
            sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
        } else if(cmd=="LOOKUP_DOC"){
            // ==========================================
            // 专供：患者 lookup <doctor> (寻找空闲时间)
            // ==========================================
            std::string doctor; ss>>doctor;
            std::cout<<"Appointment Server has received a doctor availability request."<<std::endl;
            std::vector<std::string> order;
            auto m = load_schedule_preserve(fname,order);
            auto it = m.find(doctor);
            
            if(it==m.end()){
                std::cout<<doctor<<" was not found in the system."<<std::endl;
                std::string rep = doctor + " has no time slots available\n"; 
                sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen); 
                continue;
            }
            std::cout<<doctor<<" was found in the system."<<std::endl;
            
            std::string out;
            int available_count = 0;
            for(auto &slot: it->second){
                if(slot.patient.empty()){ // 寻找没被预约的空档
                    out += slot.time + "\n";
                    available_count++;
                }
            }
            
            std::string rep;
            if (available_count == 8 || available_count == it->second.size()) {
                rep = "NO_APPTS\n"; // 对应 Client 里的 "All time blocks are available"
            } else if (available_count == 0) {
                std::string rep = doctor + " has no time slots available\n"; 

            } else {
                rep = out; 
            }
            sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);

        } else if(cmd=="VIEW_APPTS"){
           
            std::string doctor; ss>>doctor;
            std::cout<<"Appointment Server has received a request to view appointments scheduled for "<<doctor<<"."<<std::endl;
            std::vector<std::string> order;
            auto m = load_schedule_preserve(fname,order);
            auto it = m.find(doctor);
            
            if(it==m.end()){
                std::cout<<"No appointments have been made for "<<doctor<<"."<<std::endl;
                std::string rep = "NO_APPTS\n"; 
                sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen); 
                continue;
            }
            
            std::string out;
            int booked_count = 0;
            for(auto &slot: it->second){
                if(!slot.patient.empty()){ // 寻找已经被预约的档期
                    out += slot.time + "\n";
                    booked_count++;
                }
            }
            
            if (booked_count == 0) {
                std::cout<<"No appointments have been made for "<<doctor<<"."<<std::endl;
                std::string rep = "NO_APPTS\n"; 
                sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen); 
            } else {
                std::cout<<"Returning the scheduled appointments for "<<doctor<<"."<<std::endl;
                sendto(s,out.c_str(),out.size(),0,(sockaddr*)&peer,plen);
            }
        } else if(cmd=="prescribe" || cmd=="PRESCRIBE" || cmd=="GET_ILLNESS"){
            if(cmd=="GET_ILLNESS" || cmd=="prescribe" || cmd=="PRESCRIBE"){
                if(cmd=="GET_ILLNESS"){
                    std::string phash; ss>>phash;
                    std::string psuf = phash.size()>=5 ? phash.substr(phash.size()-5) : phash;
                    std::cout<<"Appointment Server has received a request from Hospital Server regarding information about a user with hash suffix "<<psuf<<" from doctor."<<std::endl;
                    std::vector<std::string> order;
                    auto m = load_schedule_preserve(fname,order);
                    bool found=false;
                    std::string illness;
                    std::string timeblk;
                    for(const auto &k: order){
                        for(auto &slot: m[k]){
                            if(slot.patient==phash){
                                illness = slot.illness.empty() ? "None" : slot.illness;
                                timeblk = slot.time;
                                slot.patient.clear();
                                slot.illness.clear();
                                found=true;
                                break;
                            }
                        }
                        if(found) break;
                    }
                    if(!found){
                        std::cout<<"Appointment Server could not find appointment for patient with hash suffix: "<<psuf<<"."<<std::endl;
                        std::string rep="NOT_FOUND\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen); continue;
                    }
                    bool ok = save_schedule_atomic_preserve(fname,m,order);
                    if(!ok){ std::string rep="GET_ILLNESS_IO_FAIL\n"; sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen); continue; }
                    std::cout<<"Sending back the requested information to the Hospital server."<<std::endl;
                    std::cout<<"Successfully removed "<<psuf<<" appointment slot, "<<timeblk<<" is now free to be scheduled for tomorrow."<<std::endl;
                    std::string rep = illness + "\n";
                    sendto(s,rep.c_str(),rep.size(),0,(sockaddr*)&peer,plen);
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