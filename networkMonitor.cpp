//networkMonitor.cpp - An network monitor
//
// 31-Jul-20  D. Jonathan      Modified

#include <fcntl.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <vector>

#define SOCKET_PATH "/tmp/a1-socket"
#define MAX_BUF     256

using namespace std;

// Socket variables
int master_fd, max_fd, rc;
fd_set active_fd_set;
fd_set read_fd_set;
int ret;
int MAX_CLIENTS, numClients=0;
int *clients;

// I/O variables
string message;
char buffer[MAX_BUF];
int len;

// Global boolean flags
bool isRunning = true;
bool isParent = true;

// User input variables
int numOfInterfaces;
vector<string> intf;

pid_t *childPid = nullptr;

void getUserInput();
void clean_up();
int createAndBindSocket();
void acceptConnections();
int write_message(std::string message, int clientSocket);
std::string read_message(int clientSocket);
static void signalHandler(int signum);

int main()
{
    struct sigaction action;

    bool should_continue = true;

    // Populate the sigaction struct's members for the handler and the mask
    action.sa_handler = signalHandler;
    sigemptyset(&(action.sa_mask));
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);

    // Retrieve number of interfaces and their names
    getUserInput();

    // Set master file descriptor to server socket listening for new connections
    master_fd = createAndBindSocket();

    // Based on user input create child intfMonitor processes
    for(int i=0; i < numOfInterfaces & isParent; i++) {
        childPid[i] = fork();

        if(childPid[i]==0) {
            isParent = false; 
            execlp("./intfMonitor", "./intfMonitor", intf.at(i).c_str(), NULL);
            cout << "child:main: pid:"<<getpid()<<" I should not get here!"<<endl;
            cout<<strerror(errno)<<endl;
        }
    }

    // Ensure we are the parent process
    // if so calls acceptConnections()
    if(isParent) {
        acceptConnections();
    }

    return 0;
}

void acceptConnections()
{
    // Clear and initialized fd sets
    FD_ZERO(&read_fd_set);
    FD_ZERO(&active_fd_set);

    // Add the master_fd to the socket set
    FD_SET(master_fd, &active_fd_set);
    max_fd = master_fd;

    // Loops until while isRunning...
    while(isRunning)
    {
        // Select from up to max_fd + 1 sockets
        read_fd_set = active_fd_set;
        ret=select(max_fd+1, &read_fd_set, NULL, NULL, NULL);
        if (ret < 0) 
        {
            cout << "server 1 : " << strerror(errno) << endl;
        }

        // Service all the sockets with input pending 
        else 
        {
            // Connection request on the master socket
            if (FD_ISSET (master_fd, &read_fd_set))
            {
                // Accept the new connection
                clients[numClients] = accept(master_fd, NULL, NULL);
                if (clients[numClients] < 0) 
                {
                     cout << "server 2: " << strerror(errno) << endl;
                } 
                else 
                {
                    // Add the new connection to the set
                    FD_SET (clients[numClients], &active_fd_set);

                    // Sends message "Monitor" to client (intfMonitor)
                    // to start displaying interface monitoring statistics
                    write_message("Monitor", numClients);

                    // Update the maximum fd
                    if(max_fd<clients[numClients]) max_fd=clients[numClients];
                    ++numClients;
                }
            }

            // Data arriving on an already-connected socket
            else 
            {
                // Find which client sent the data
                for (int i = 0; i < numClients; i++) 
                {
                    if (FD_ISSET (clients[i], &read_fd_set)) {
                        
                        // Read message from client interface
                        message = read_message(i);
                        
                        // If the client interface monitor returns "Link Down"
                        // we will message the that client interface to restore the link
                        // and begin monitoring and displaying interface statistics again
                        if(message.compare("Link Down") == 0)
                        {
                            write_message("Set Link Up", i);
                            write_message("Monitor", i);
                        }
                        // Prints out status of an interface. Eg: "Link Down", "Link Up", "Monitoring"...
                        cout << "Interface " << intf.at(i).c_str() << ": " << message <<endl;
                    }
                }
            }
        }
    }
    // This line executes when the Network Monitor has received
    // a ctrl+c which is handled by the Sighandler which sets isRunning
    // to false breaking the loop
    clean_up();
}

// Sends a given message to the interface monitor through the socket
int write_message(std::string message, int clientSocket)
{
    int bytes_written = 0;

    // Clear the buffer of existing data and then copy the message into it
    memset(buffer, '\0', sizeof(buffer));
    strncpy(buffer, message.c_str(), sizeof(buffer) - 1);

    // Send the buffer over the socket file using write()
    bytes_written = write(clients[clientSocket], buffer, sizeof(buffer));

    if(bytes_written ==-1) {
        cout << "Networking Monitor write error: " << strerror(errno) << endl;
    }
    return bytes_written;
}

// Receives any incoming message from the interface monitor and returns it as a string
std::string read_message(int clientSocket)
{
    std::string message;

    // Clear the buffer of any existing data and then read incoming data from
    // the socket file into it
    memset(buffer, '\0', sizeof(buffer));
    size_t bytes_recieved = read(clients[clientSocket], buffer, sizeof(buffer));

    if(bytes_recieved == -1) {
        cout<<"server: Read Error"<<endl;
        cout<<strerror(errno)<<endl;
    }

    message = buffer;

    return message;
}

// Create and bind a socket to act as the server for our interface monitors to connect to
int createAndBindSocket()
{
    struct sockaddr_un addr;
    int rc;

    //Create the socket
    memset(&addr, 0, sizeof(addr));
    if ((rc = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        cout << "server: " << strerror(errno) << endl;
        exit(-1);
    }

    //Set the socket path to a local socket file
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);
    unlink(SOCKET_PATH);

    //Bind the socket
    if (bind(rc, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        cout << "server: " << strerror(errno) << endl;
        close(rc);
        exit(-1);
    }

    cout<<"Waiting for the client..."<<endl;
    //Listen for a client to connect to this local socket file
    if (listen(rc, numOfInterfaces) == -1) {
        cout << "server: " << strerror(errno) << endl;
        unlink(SOCKET_PATH);
        close(rc);
        exit(-1);
    }

    return rc;
}

void getUserInput(){
    // Query user for number of interfaces
    cout << "How many interfaces would you like to instantiate: ";
    cin >> numOfInterfaces;
    
    // Ensure input is a number
    while(cin.fail()){
        cout << "Error: Please enter a valid number" << endl;
        cin.clear();
        cin.ignore(256, '\n');
        cin >> numOfInterfaces;
    }

    // Allocate the childPid array based on user input
    childPid = new pid_t[numOfInterfaces];

    // Set the maximum amount of clients able to connect 
    // to the number interfaces user has input
    MAX_CLIENTS = numOfInterfaces;
    clients = new int[MAX_CLIENTS];

    // Query user for the name of each interface 
    for(int i = 0; i < numOfInterfaces; i++){
        
        cout << "Please enter the name of interface " << i << ": ";
        cin.clear();
        string in;
        cin >> in;
        
        while(cin.fail()){
            cout << "Error: Please enter a valid interface name" << endl;
            cin.clear();
            cin.ignore(256, '\n');
            cin >> in;
        }
        // Store interface names into a vector
        intf.push_back(in);
    }
    
    
}

// The signal handler (only used for SIGINT)
static void signalHandler(int signum)
{
    // If user inputs ctrl+c program will exit socket communications loop
    // and begin shutting down child processes and cleanup program resources
    switch(signum) {
        case SIGINT:
            isRunning = false;
	    break;
        default:
            cout<<"NetworkMonitor: Undefined signal"<<endl;
    }
}

// Shuts down child intfMonitor processes and cleans up program resources
void clean_up()
{  
    // Loop through all clients
    for(int i = 0; i < numClients; i++)
    {
        // Tell all client intfMonitors to shut down and clean up
        write_message("Shut Down", i);

        //Give some time for shutdown process
        sleep(1);

        // On shutdown intfMonitor may send a "Done" message
        // read the message
        message = read_message(i);
        
        // If "Done" is received close socket connection to the client
        if(message.compare("Done")){
            FD_CLR(clients[i], &active_fd_set);
            close(clients[i]);
        }
        // If "Done" isn't received close socket connection to the client
        else{
            FD_CLR(clients[i], &active_fd_set);
            close(clients[i]);
        }
    }

    // Clean up program resources
    delete [] childPid;
    delete [] clients;

    // Close master file descriptor to socket
    close(master_fd);

    // Unlink socket path
    unlink(SOCKET_PATH);
}