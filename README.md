EE450 Project

Name: Wang Yinglin
Student ID: 7462174465

Build:
make clean && make

What I have done:
I implemented the complete client-server hospital system for this assignment. The project includes one client, one hospital server, and three backend servers for authentication, appointments, and prescriptions. The client talks to the hospital server through TCP, and the hospital server communicates with the backend servers through UDP. I also added username/password hashing on the client side before authentication, support for both patient and doctor commands, appointment scheduling and cancellation, prescription lookup, and prescription generation based on the illness stored with the appointment.

Code files:
- `client.cpp`: starts the client, hashes the username and password, sends requests to the hospital server, and prints the user-facing output for both patient and doctor commands.
- `hospital_server.cpp`: the central server. It accepts TCP requests from the client, determines whether the user is a doctor or a patient, and forwards different requests to the corresponding backend server through UDP.
- `authentication_server.cpp`: checks the received credential information against the user database and returns whether authentication succeeds.
- `appointment_server.cpp`: manages appointment lookup, scheduling, cancellation, viewing existing appointments, and returning the illness information associated with an appointment.
- `prescription_server.cpp`: stores prescriptions and returns prescription records when a patient or doctor asks to view them.
- `sha256.cpp`: SHA-256 implementation used by the client to hash usernames and passwords before sending authentication requests.
- `sha256.h`: header file for the SHA-256 functions and wrapper class used by the client.

Message format:
- All messages are plain-text messages.
- In general, one command word is followed by its parameters, and different fields are separated by a single space.
- For authentication, the client sends `AUTH <username_hash> <password_hash>` to the hospital server.
- For appointment and prescription related operations, the message format is also command + space-separated fields, such as patient hash, doctor name, time, illness, treatment, or frequency depending on the request.
- Most server replies are plain-text status messages such as `SUCCESS`, `FAIL`, `NO_APPT`, or `NO_PRESC`. When multiple results need to be returned, the program uses newline-separated text, and in a few prescription-related replies it uses `|` as a separator.

Idiosyncrasies / limitations:
- This project is written for local execution on `127.0.0.1`. It is not designed for remote hosts.
- The program assumes all backend servers are already running before the client sends requests. If a required server is not running, the request may fail or return no useful response.
- The program depends on the expected input files used by the servers. If those files are missing or not in the expected format, the related function will fail.
- Some command parameters are parsed as single tokens separated by spaces, so doctor names, illness names, treatments, and frequency values should follow the format expected by the provided data files and commands.
- The hospital server waits for UDP replies with a short timeout. If a backend server does not reply in time, the result may be incomplete.

Reused code:
Yes. I reused the SHA-256 implementation in `sha256.cpp` and `sha256.h`. The original code is from LekKit: https://github.com/LekKit. The source files already include the original license and copyright notice.

Ubuntu version:
Ubuntu 20.04# Hospital-server
