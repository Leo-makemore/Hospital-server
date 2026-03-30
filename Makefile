CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -O2

all: authentication_server appointment_server prescription_server hospital_server client

authentication_server: authentication_server.cpp
	$(CXX) $(CXXFLAGS) -o authentication_server authentication_server.cpp

appointment_server: appointment_server.cpp
	$(CXX) $(CXXFLAGS) -o appointment_server appointment_server.cpp

prescription_server: prescription_server.cpp
	$(CXX) $(CXXFLAGS) -o prescription_server prescription_server.cpp

hospital_server: hospital_server.cpp
	$(CXX) $(CXXFLAGS) -o hospital_server hospital_server.cpp

client: client.cpp sha256.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp sha256.cpp

clean:
	rm -f authentication_server appointment_server prescription_server hospital_server client