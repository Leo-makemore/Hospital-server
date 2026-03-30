#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sha256.h"

struct Res {
    std::string text;
    int port;
};

static Res send_cmd(const std::string &msg) {
    Res r;
    r.port = -1;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) return r;
    
    sockaddr_in addr; 
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(26465); // 26000 + 465
    
    if(connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return r;
    }
    
    // grab the dynamic client port right after connecting
    sockaddr_in local;
    socklen_t len = sizeof(local);
    if(getsockname(sock, (sockaddr*)&local, &len) == 0) {
        r.port = ntohs(local.sin_port);
    }

    write(sock, msg.c_str(), msg.size());
    
    char buf[8192];
    ssize_t n = read(sock, buf, sizeof(buf)-1);
    if(n > 0) {
        buf[n] = 0;
        r.text = buf;
    }
    
    close(sock);
    return r;
}

int main(int argc, char** argv) {
    if(argc != 3) {
        std::cout << "Usage: ./client <username> <password>" << std::endl;
        return 0;
    }

    std::string uname = argv[1];
    std::string pwd = argv[2];

    std::cout << "The client is up and running." << std::endl;

    char uhash[65] = {0}, phash[65] = {0};
    sha256_easy_hash_hex(uname.c_str(), uname.size(), uhash);
    sha256_easy_hash_hex(pwd.c_str(), pwd.size(), phash);

    std::cout << uname << " sent an authentication request to the hospital server." << std::endl;

    std::string auth_req = std::string("AUTH ") + uhash + " " + phash;
    Res res = send_cmd(auth_req);

    bool is_doc = false;
    
    // parse authentication result
    if(res.text.find("SUCCESS") != std::string::npos) {
        if(res.text.find("DOCTOR") != std::string::npos || res.text.find("doctor") != std::string::npos) {
            is_doc = true;
            std::cout << uname << " received the authentication result. Authentication successful. You have been granted doctor access." << std::endl;
        } else {
            std::cout << uname << " received the authentication result. Authentication successful. You have been granted patient access." << std::endl;
        }
    } else {
        std::cout << "The credentials are incorrect. Please try again." << std::endl;
        return 0;
    }

    // main command loop
    while(true) {
        std::string cmd_line;
        if(!std::getline(std::cin, cmd_line)) break;
        if(cmd_line.empty()) continue;

        std::stringstream ss(cmd_line);
        std::string cmd;
        ss >> cmd;

        if(cmd == "quit") {
            std::cout << "You have successfully been logged out." << std::endl;
            std::cout << "-Quit Program-" << std::endl;
            break;
        }
        
        if(cmd == "help") {
            if(is_doc) {
                std::cout << "Please enter the command:\n<view_appointments>,\n<prescribe <patient> <frequency>>,\n<view_prescription>,\n<quit>" << std::endl;
            } else {
                std::cout << "Please enter the command:\n<lookup>,\n<lookup <doctor>>,\n<schedule <doctor> <start_time> <illness>>,\n<cancel>,\n<view_appointment>,\n<view_prescription>,\n<quit>" << std::endl;
            }
            continue;
        }

        // patient commands
        if(!is_doc) {
            if(cmd == "lookup") {
            std::string doc;
            ss >> doc;
            if(doc.empty()) {
                std::cout << uname << " sent a lookup request to the hospital server." << std::endl;
                Res reply = send_cmd(std::string("lookup ") + uhash); 
                
                std::cout << "The client received the response from the hospital server using TCP over port " << reply.port << "." << std::endl;
                if(reply.text.find("NO_DOCTORS") != std::string::npos) {
                } else {
                    std::cout << "The following doctors are available:\n" << reply.text;
                }
            } else {
                std::cout << "Patient " << uname << " sent a lookup request to the hospital server for " << doc << "." << std::endl;
                Res reply = send_cmd(std::string("lookup ") + uhash + " " + doc); 
                
                std::cout << "The client received the response from the hospital server using TCP over port " << reply.port << "." << std::endl;
                if(reply.text.find("NO_APPTS") != std::string::npos) {
                    std::cout << "All time blocks are available for " << doc << "." << std::endl;
                } else {
                    std::cout << doc << " is available at times:\n" << reply.text;
                }
            }
        }
            else if(cmd == "schedule") {
                std::string doc_n, time_n, ill_n;
                ss >> doc_n >> time_n >> ill_n; 
                
                if(doc_n.empty() || time_n.empty() || ill_n.empty()) {
                    std::cout << "Invalid format. Please use: schedule <doctor> <time> <illness>" << std::endl;
                    continue; 
                }

                std::cout << uname << " sent an appointment schedule request to the hospital server." << std::endl;
                
                std::string req = std::string("schedule ") + uhash + " " + doc_n + " " + time_n + " " + ill_n;
                Res reply = send_cmd(req); 
                
                std::cout << "The client received the response from the Hospital Server using TCP over port " << reply.port << std::endl;
                
                if(reply.text.find("SUCCESS") != std::string::npos || reply.text.find("SCHEDULE_OK") != std::string::npos) {
                    std::cout << "An appointment has been successfully scheduled for patient " << uname << " with " << doc_n << " at " << time_n << "." << std::endl;
                } else if(reply.text.find("CONFLICT") != std::string::npos || reply.text.find("FULL") != std::string::npos) {
                    std::cout << "Unable to schedule an appointment with " << doc_n << " at this time, as all time blocks have been taken up." << std::endl;
                } else {
                    std::cout << "Unable to schedule an appointment with " << doc_n << " at " << time_n << ". Other available time blocks are\n" << reply.text;
                }
            }
            else if(cmd == "cancel") {
                std::cout << uname << " sent a cancellation request to the Hospital Server." << std::endl;
                Res reply = send_cmd(cmd_line + " " + uhash);
                
                std::cout << "The client received the response from the Hospital Server using TCP over port " << reply.port << std::endl;
                if(reply.text.find("NO_APPT") != std::string::npos) {
                    std::cout << "You have no appointments available to cancel." << std::endl;
                } else {
                    std::stringstream rss(reply.text);
                    std::string doc_n, time_n;
                    rss >> doc_n >> time_n;
                    std::cout << "You have successfully cancelled your appointment with " << doc_n << " at " << time_n << "." << std::endl; 
                }
            }
            else if(cmd == "view_appointment") {
                std::cout << uname << " sent a request to view their appointment to the Hospital Server." << std::endl;
                Res reply = send_cmd(cmd_line + " " + uhash); 
                
                std::cout << "The client received the response from the hospital server using TCP over port " << reply.port << std::endl;
                if(reply.text.find("NO_APPT") != std::string::npos) {
                    std::cout << "You do not have an appointment today." << std::endl;
                } else {
                    std::stringstream rss(reply.text);
                    std::string doc_n, time_n;
                    rss >> doc_n >> time_n;
                    std::cout << "You have an appointment scheduled with " << doc_n << " at " << time_n << "." << std::endl;
                }
            }
            else if(cmd == "view_prescription") {
                std::cout << uname << " sent a request to view their prescription to the Hospital Server." << std::endl;
                
                Res reply = send_cmd(std::string("view_prescription PATIENT ") + uhash);
                
                std::cout << "The client received the response from the hospital server using TCP over port " << reply.port << std::endl;
                if(reply.text.find("NO_PRESC") != std::string::npos || reply.text.find("NONE") != std::string::npos) {
                    std::cout << "You do not have a prescription to look up." << std::endl;
                } else {
                    std::string raw = reply.text;
                    size_t pos = raw.find('|');
                    if (pos != std::string::npos) {
                        raw = raw.substr(0, pos); 
                    }
                    std::stringstream rss(raw);
                    std::string doc_n, phash_n, treat_n, freq_n;
                    rss >> doc_n >> phash_n >> treat_n >> freq_n; 
                    
                    std::cout << "You have been prescribed " << treat_n << "." << std::endl; 
                }
            }
        } 
        // doctor commands
        else {
            if(cmd == "view_appointments") {
                std::cout << uname << " sent a request to view their scheduled appointments to the Hospital Server." << std::endl;
                Res reply = send_cmd(cmd_line + " " + uname); // docs look up themselves
                
                std::cout << "The client received the response from the hospital server using TCP over port " << reply.port << std::endl;
                if(reply.text.find("NO_APPTS") != std::string::npos) {
                    std::cout << "You do not have any appointments scheduled." << std::endl;
                } else {
                    std::cout << uname << " is scheduled at times:\n" << reply.text;
                }
            }
            else if(cmd == "prescribe") {
                std::string pat, freq; 
                ss >> pat >> freq; 
                
                char pat_hash[65] = {0};
                sha256_easy_hash_hex(pat.c_str(), pat.size(), pat_hash);

                std::cout << uname << " sent a request to the Hospital Server to prescribe " << pat << " following their diagnosis." << std::endl;
                
                Res reply = send_cmd(std::string("prescribe ") + uname + " " + pat_hash + " " + freq);
                std::cout << "The client received the response from the hospital server using TCP over port " << reply.port << std::endl;
                
                std::string treatment = reply.text;
                if (!treatment.empty() && treatment.back() == '\n') treatment.pop_back(); 
                
                std::cout << "You have successfully prescribed " << pat << " with " << treatment << ", to be taken " << freq << "." << std::endl;
            }
            else if(cmd == "view_prescription") {
                std::string pat; ss >> pat;
                
                char pat_hash[65] = {0};
                sha256_easy_hash_hex(pat.c_str(), pat.size(), pat_hash);

                std::cout << uname << " sent a request to view " << pat << " prescription to the Hospital Server." << std::endl;
                
                Res reply = send_cmd(std::string("view_prescription DOCTOR ") + uname + " " + pat_hash);
                
                std::cout << "The client received the response from the hospital server using TCP over port " << reply.port << std::endl;
                
                if(reply.text.find("NO_PRESC") != std::string::npos || reply.text.find("NONE") != std::string::npos) {
                    std::cout << pat << " does not have a prescription." << std::endl;
                } else {
                    std::string raw = reply.text;
                    size_t pos = raw.find('|');
                    if (pos != std::string::npos) {
                        raw = raw.substr(0, pos); 
                    }
                    std::stringstream rss(raw);
                    std::string doc_n, phash_n, treat_n, freq_n;
                    rss >> doc_n >> phash_n >> treat_n >> freq_n; 
                    
                    std::cout << pat << " has been prescribed " << treat_n << "." << std::endl;
                }
            }
        }
    }

    return 0;
}