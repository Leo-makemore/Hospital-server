#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fstream>

std::string trim(const std::string &s){
    size_t a = 0, b = s.size();
    while(a < b && isspace((unsigned char)s[a])) a++;
    while(b > a && isspace((unsigned char)s[b-1])) b--;
    return s.substr(a, b - a);
}

std::map<std::string, std::string> load_treatments(const std::string &fname){
    std::map<std::string, std::string> m;
    std::ifstream fi(fname);
    std::string line;
    bool inTreat = false;
    while(std::getline(fi, line)){
        std::string t = trim(line);
        if(t.empty()) continue;
        if(t == "[Treatments]"){ inTreat = true; continue; }
        if(inTreat){
            std::istringstream ss(t);
            std::string illness, treat;
            ss >> illness; std::getline(ss, treat); treat = trim(treat);
            if(!illness.empty() && !treat.empty()) m[illness] = treat;
        }
    }
    return m;
}

std::string get_suffix(const std::string &hash) {
    if(hash.size() >= 5) return hash.substr(hash.size() - 5);
    return hash;
}

bool is_doctor(const std::string& h) {
    std::ifstream fi("hospital.txt");
    std::string line;
    bool inDoc = false;
    while(std::getline(fi, line)) {
        std::string t = trim(line);
        if(t == "[Doctors]") { inDoc = true; continue; }
        if(t == "[Treatments]") { inDoc = false; continue; }
        if(inDoc && t.find(h) != std::string::npos) return true;
    }
    return false;
}

int main(){
    const int auth_port = 21465;
    const int appt_port = 23465;
    const int presc_port = 22465;
    const int hosp_udp_port = 25465;
    const int hosp_tcp_port = 26465;

    int usock = socket(AF_INET, SOCK_DGRAM, 0);
    if(usock < 0) return 1;
    sockaddr_in a; std::memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_addr.s_addr = inet_addr("127.0.0.1"); a.sin_port = htons(hosp_udp_port);
    if(bind(usock, (sockaddr*)&a, sizeof(a)) < 0) return 1;


    int tfd = socket(AF_INET, SOCK_STREAM, 0);
    if(tfd < 0) return 1;
    int opt = 1; setsockopt(tfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in ta; std::memset(&ta, 0, sizeof(ta));
    ta.sin_family = AF_INET; ta.sin_addr.s_addr = inet_addr("127.0.0.1"); ta.sin_port = htons(hosp_tcp_port);
    if(bind(tfd, (sockaddr*)&ta, sizeof(ta)) < 0) return 1;
    if(listen(tfd, 5) < 0) return 1;

    sockaddr_in bound_udp, bound_tcp;
    socklen_t len_udp = sizeof(bound_udp);
    socklen_t len_tcp = sizeof(bound_tcp);

    if (getsockname(usock, (struct sockaddr *)&bound_udp, &len_udp) == -1 || 
        getsockname(tfd, (struct sockaddr *)&bound_tcp, &len_tcp) == -1) {
        return 1;
    }

    int actual_udp_port = ntohs(bound_udp.sin_port);
    int actual_tcp_port = ntohs(bound_tcp.sin_port);

    std::cout << "Hospital Server is up and running using UDP on port " << actual_udp_port << "." << std::endl;
    auto treatments = load_treatments("hospital.txt");

    while(true){
        sockaddr_in cli; socklen_t cln = sizeof(cli);
        int c = accept(tfd, (sockaddr*)&cli, &cln);
        if(c < 0) continue;
        char buf[8192]; ssize_t r = read(c, buf, sizeof(buf)-1);
        if(r <= 0){ close(c); continue; }
        buf[r] = 0; 
        std::string req(buf); 
        std::stringstream ss(req); 
        std::string cmd; ss >> cmd;

        if(cmd == "AUTH"){
            std::string uh, ph; ss >> uh >> ph;
            std::string suff = get_suffix(uh);
            std::cout << "Hospital Server received an authentication request from a user with hash suffix " << suff << "." << std::endl;
            
            std::string out = std::string("AUTH_CHECK ") + uh + " " + ph;
            sockaddr_in dest; std::memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(auth_port);
            sendto(usock, out.c_str(), out.size(), 0, (sockaddr*)&dest, sizeof(dest));
            
            std::cout << "Hospital Server has sent an authentication request to the Authentication Server." << std::endl;
            
            char rbuf[256]; sockaddr_in from; socklen_t fl = sizeof(from);
            ssize_t rr = recvfrom(usock, rbuf, sizeof(rbuf)-1, 0, (sockaddr*)&from, &fl);
            if(rr > 0){
                rbuf[rr] = 0; std::string resp(rbuf);
                std::cout << "Hospital server has received the response from the authentication server using UDP over port " << actual_udp_port << "." << std::endl;
                
                if(resp.find("SUCCESS") != std::string::npos) {
                    std::cout << "User with a hash suffix " << suff << " has been granted access to the system. Determining the access of the user." << std::endl;
                    
                    if(is_doctor(uh)) {
                        std::cout << "User with hash suffix " << suff << " will be granted doctor access." << std::endl;
                        
                        resp += " DOCTOR"; 
                        
                    } else {
                        std::cout << "User with hash " << suff << " will be granted patient access." << std::endl;
                        
                        resp += " PATIENT"; 
                    }
                }
                
                write(c, resp.c_str(), resp.size());
            }
            std::cout << "Hospital Server has sent the response from Authentication Server to the client using TCP over port " << actual_tcp_port << "." << std::endl;

        } else if(cmd == "lookup"){
            std::string uhash, doc; ss >> uhash >> doc;
            if(doc.empty()){
                std::cout << "Hospital Server received a lookup request from a user with a hash suffix " << get_suffix(uhash) << " over port " << actual_tcp_port << "." << std::endl;
                
                std::ifstream fi("hospital.txt");
                std::string line;
                std::string out;
                bool inDoc = false;
                while(std::getline(fi, line)){
                    std::string t = trim(line);
                    if(t == "[Doctors]"){ inDoc = true; continue; }
                    if(t == "[Treatments]"){ break; } 
                    if(inDoc){
                        if(t.empty()) continue; 
                        std::istringstream docs(t);
                        std::string docname; 
                        docs >> docname; 
                        out += docname + "\n";
                    }
                }
                if(out.empty()) out = "NO_DOCTORS\n";
                write(c, out.c_str(), out.size());
                std::cout << "Hospital Server has sent the doctor lookup to the client." << std::endl;
            } else {
                std::cout << "Hospital Server has received a lookup request from a user with hash suffix " << get_suffix(uhash) << " to lookup " << doc << " availability using TCP over port " << actual_tcp_port << "." << std::endl;
                
                std::string out = std::string("LOOKUP_DOC ") + doc;
                sockaddr_in dest; std::memset(&dest, 0, sizeof(dest));
                dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(appt_port);
                sendto(usock, out.c_str(), out.size(), 0, (sockaddr*)&dest, sizeof(dest));
                
                std::cout << "Hospital Server sent the doctor lookup request to the Appointment server." << std::endl;
                
                fd_set set; FD_ZERO(&set); FD_SET(usock, &set);
                struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
                int rv = select(usock+1, &set, NULL, NULL, &tv);
                if(rv > 0){
                    char rbuf[8192]; sockaddr_in from; socklen_t fl = sizeof(from);
                    ssize_t rr = recvfrom(usock, rbuf, sizeof(rbuf)-1, 0, (sockaddr*)&from, &fl);
                    if(rr > 0){ rbuf[rr] = 0; std::string rep(rbuf); write(c, rep.c_str(), rep.size()); }
                }
                std::cout << "Hospital Server has received the response from Appointment Server using UDP over port " << actual_udp_port << "." << std::endl;
                std::cout << "The Hospital Server has sent the response to the client." << std::endl;
            }

        } else if(cmd == "schedule" ||cmd == "SCHEDULE"){
            std::string uhash, doc, time, illness; ss >> uhash >> doc >> time >> illness;
            std::cout << "Hospital Server has received a schedule request from a user with hash suffix: " << get_suffix(uhash) << " to book an appointment using TCP over port " << actual_tcp_port << "." << std::endl;
            
            std::string out = std::string("SCHEDULE ") + doc + " " + time + " " + uhash + " " + illness;
            sockaddr_in dest; std::memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(appt_port);
            sendto(usock, out.c_str(), out.size(), 0, (sockaddr*)&dest, sizeof(dest));
            
            std::cout << "Hospital Server has sent the schedule request to the appointment server." << std::endl;
            
            fd_set set; FD_ZERO(&set); FD_SET(usock, &set);
            struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
            int rv = select(usock+1, &set, NULL, NULL, &tv);
            if(rv > 0){
                char rbuf[512]; sockaddr_in from; socklen_t fl = sizeof(from);
                ssize_t rr = recvfrom(usock, rbuf, sizeof(rbuf)-1, 0, (sockaddr*)&from, &fl);
                if(rr > 0){ rbuf[rr] = 0; write(c, rbuf, rr); }
            }
            std::cout << "Hospital Server has received the response from Appointment Server using UDP over " << actual_udp_port << "." << std::endl;
            std::cout << "The hospital server has sent the response to the client." << std::endl;

        } else if(cmd == "cancel" ||cmd == "CANCEL"){
            std::string uhash; ss >> uhash;
            std::cout << "Hospital Server has received a cancel request from user with hash suffix: " << get_suffix(uhash) << " to cancel their appointment using TCP over port " << actual_tcp_port << "." << std::endl;
            
            std::string out = std::string("CANCEL ") + uhash;
            sockaddr_in dest; std::memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(appt_port);
            sendto(usock, out.c_str(), out.size(), 0, (sockaddr*)&dest, sizeof(dest));
            
            std::cout << "The hospital server has sent the cancel request to the appointment server." << std::endl;
            
            fd_set set; FD_ZERO(&set); FD_SET(usock, &set);
            struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
            int rv = select(usock+1, &set, NULL, NULL, &tv);
            if(rv > 0){
                char rbuf[512]; sockaddr_in from; socklen_t fl = sizeof(from);
                ssize_t rr = recvfrom(usock, rbuf, sizeof(rbuf)-1, 0, (sockaddr*)&from, &fl);
                if(rr > 0){ rbuf[rr] = 0; write(c, rbuf, rr); }
            }
            std::cout << "Hospital Server has received the response from Appointment Server using UDP over port " << actual_udp_port << "." << std::endl;
            std::cout << "The hospital server has sent the response to the client." << std::endl;

        } else if(cmd == "view_appointment" ||cmd == "VIEW_APPOINTMENT"){
            std::string uhash; ss >> uhash;
            std::cout << "Hospital server has received a view appointment request from a user with hash suffix " << get_suffix(uhash) << " to view their appointment details using TCP over port " << actual_tcp_port << "." << std::endl;
            
            std::string out = std::string("GET_APPT_FOR ") + uhash;
            sockaddr_in dest; std::memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(appt_port);
            sendto(usock, out.c_str(), out.size(), 0, (sockaddr*)&dest, sizeof(dest));
            
            std::cout << "Hospital Server has sent the view appointments request to the Appointment Server." << std::endl;
            
            fd_set set; FD_ZERO(&set); FD_SET(usock, &set);
            struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
            int rv = select(usock+1, &set, NULL, NULL, &tv);
            if(rv > 0){
                char rbuf[8192]; sockaddr_in from; socklen_t fl = sizeof(from);
                ssize_t rr = recvfrom(usock, rbuf, sizeof(rbuf)-1, 0, (sockaddr*)&from, &fl);
                if(rr > 0){ rbuf[rr] = 0; std::string rep(rbuf); write(c, rep.c_str(), rep.size()); }
            }
            std::cout << "Hospital Server has received the response from the appointment server using UDP over port " << actual_udp_port << "." << std::endl;
            std::cout << "The hospital server has sent the response to the client." << std::endl;

        } else if(cmd == "view_appointments" ||cmd == "VIEW_APPTS"){
            std::string doc; ss >> doc;
            std::cout << "Hospital Server has received a view appointments request from " << doc << " to view their schedule details using TCP over port " << actual_tcp_port << "." << std::endl;
            
            std::string out = std::string("VIEW_APPTS ") + doc;
            sockaddr_in dest; std::memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(appt_port);
            sendto(usock, out.c_str(), out.size(), 0, (sockaddr*)&dest, sizeof(dest));
            
            std::cout << "The hospital server has sent the view appointments request to the Appointment Server." << std::endl;
            
            fd_set set; FD_ZERO(&set); FD_SET(usock, &set);
            struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
            int rv = select(usock+1, &set, NULL, NULL, &tv);
            if(rv > 0){
                char rbuf[8192]; sockaddr_in from; socklen_t fl = sizeof(from);
                ssize_t rr = recvfrom(usock, rbuf, sizeof(rbuf)-1, 0, (sockaddr*)&from, &fl);
                if(rr > 0){ rbuf[rr] = 0; std::string rep(rbuf); write(c, rep.c_str(), rep.size()); }
            }
            std::cout << "Hospital server has received the response from the Appointment server using UDP over port " << actual_udp_port << "." << std::endl;
            std::cout << "The hospital server has sent the response to the client." << std::endl;

        } else if(cmd == "prescribe" ||cmd == "PRESCRIBE"){
            std::string doc, uhash, freq; ss >> doc >> uhash >> freq;
            std::cout << "Hospital Server has received a prescription request from " << doc << " for a user with hash suffix " << get_suffix(uhash) << " using TCP over port " << actual_tcp_port << "." << std::endl;
            
            std::string out = std::string("GET_ILLNESS ") + uhash;
            sockaddr_in dest; std::memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(appt_port);
            sendto(usock, out.c_str(), out.size(), 0, (sockaddr*)&dest, sizeof(dest));
            
            std::cout << "Hospital Server has sent a request to fetch patients with hash suffix " << get_suffix(uhash) << " illness information to the Appointment Server." << std::endl;
            
            fd_set set; FD_ZERO(&set); FD_SET(usock, &set);
            struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
            int rv = select(usock+1, &set, NULL, NULL, &tv);
            std::string illness;
            if(rv > 0){
                char rbuf[1024]; sockaddr_in from; socklen_t fl = sizeof(from);
                ssize_t rr = recvfrom(usock, rbuf, sizeof(rbuf)-1, 0, (sockaddr*)&from, &fl);
                if(rr > 0){
                    rbuf[rr] = 0; std::string resp(rbuf); resp = trim(resp);
                    illness = resp;
                }
            }
            std::cout << "Hospital Server has received the illness response from the Appointment server using UDP over port " << actual_udp_port << "." << std::endl;
            
            std::string treatment = "Unknown";
            auto it = treatments.find(illness);
            if(it != treatments.end()) treatment = it->second;
            
            std::cout << "Acquiring treatment for " << illness << " from the database." << std::endl;
            
            std::string preq = std::string("PRESCRIBE ") + doc + " " + uhash + " " + treatment + " " + freq;
            std::memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(presc_port);
            sendto(usock, preq.c_str(), preq.size(), 0, (sockaddr*)&dest, sizeof(dest));
            
            std::cout << "Hospital server has sent the prescription request to the prescription server to prescribe " << treatment << "." << std::endl;
            
            FD_ZERO(&set); FD_SET(usock, &set);
            tv.tv_sec = 1; tv.tv_usec = 0;
            rv = select(usock+1, &set, NULL, NULL, &tv);
            if(rv > 0){
                char rbuf[2048]; sockaddr_in from; socklen_t fl = sizeof(from);
                ssize_t rr = recvfrom(usock, rbuf, sizeof(rbuf)-1, 0, (sockaddr*)&from, &fl);
                if(rr > 0){ 
                    rbuf[rr] = 0; 
                    std::string rep = treatment + "\n"; 
                    write(c, rep.c_str(), rep.size()); 
                }
            }
            std::cout << "Hospital server has received the response from the prescription server using UDP over port " << actual_udp_port << std::endl;
            std::cout << "The hospital server has sent the response to the client." << std::endl;

        } else if(cmd == "view_prescription" ||cmd == "VIEW_PRESC"){
            std::string role; ss >> role;
            if(role == "PATIENT") {
                std::string uhash; ss >> uhash;
                std::cout << "Hospital Server has received a prescription request from a patient with hash suffix " << get_suffix(uhash) << " to view their prescription details using TCP over port " << actual_tcp_port << "." << std::endl;
                
                std::string out = std::string("VIEW_PRESC ") + uhash;
                sockaddr_in dest; std::memset(&dest, 0, sizeof(dest));
                dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(presc_port);
                sendto(usock, out.c_str(), out.size(), 0, (sockaddr*)&dest, sizeof(dest));
            } else {
                std::string doc, uhash; ss >> doc >> uhash;
                std::cout << "Hospital Server has received a prescription request from " << doc << " to view a patient with hash suffix " << get_suffix(uhash) << " prescription details using TCP over port " << actual_tcp_port << "." << std::endl;
                
                std::string out = std::string("VIEW_PRESC ") + uhash;
                sockaddr_in dest; std::memset(&dest, 0, sizeof(dest));
                dest.sin_family = AF_INET; dest.sin_addr.s_addr = inet_addr("127.0.0.1"); dest.sin_port = htons(presc_port);
                sendto(usock, out.c_str(), out.size(), 0, (sockaddr*)&dest, sizeof(dest));
            }
            
            std::cout << "Hospital Server has sent the prescription request to the Prescription Server." << std::endl;
            
            fd_set set; FD_ZERO(&set); FD_SET(usock, &set);
            struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
            int rv = select(usock+1, &set, NULL, NULL, &tv);
            if(rv > 0){
                char rbuf[8192]; sockaddr_in from; socklen_t fl = sizeof(from);
                ssize_t rr = recvfrom(usock, rbuf, sizeof(rbuf)-1, 0, (sockaddr*)&from, &fl);
                if(rr > 0){ rbuf[rr] = 0; std::string rep(rbuf); write(c, rep.c_str(), rep.size()); }
            }
            std::cout << "Hospital server has received the response from the prescription server using UDP over port " << actual_udp_port << "." << std::endl;
            std::cout << "Hospital server has sent the response to the client." << std::endl;
        }
        close(c);
    }
    close(tfd); close(usock); return 0;
}