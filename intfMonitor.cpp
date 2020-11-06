//intfMonitor_solution.cpp - An interface monitor
//
// 13-Jul-20  M. Watler        Created.
// 31-Jul-20  L. Kloosterman   Modified
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

const int MAXBUF=128;

// This will be reference to the socket used for communication with the network
// monitor
int socket_descriptor = -1;

bool isRunning = true;

char buffer[MAXBUF];

// The path to the socket file
const std::string socket_file_pathname = "/tmp/a1-socket";

// The interface directory "root" path
std::string interface_directory = "/sys/class/net/";

static void signalHandler(int signal);

// Establishes a connection to the network monitor using the
// socket_file_pathname
int make_connection()
{
    struct sockaddr_un address;

    // Create a new AF_UNIX socket for inter-process communication
    int socket_d = socket(AF_UNIX, SOCK_STREAM, 0);

    if (socket_d != -1)
    {
        // Set the family and the path to the socket file in the address struct
        memset(&address, 0, sizeof(struct sockaddr_un));
        address.sun_family = AF_UNIX;
        strncpy(address.sun_path, socket_file_pathname.c_str(), sizeof(address.sun_path) - 1);

        // Establish a connection to the socket file
        int return_code = connect(socket_d, (struct sockaddr *)&address, sizeof(struct sockaddr_un));

        if (return_code == -1) {
            socket_d = -1;
        }
    }

    return socket_d;
}

// Cleans up the process by closing the connected socket after sending the
// "Done" message to the network monitor
void clean_up()
{
    memset(buffer, '\0', sizeof(buffer));
    strncpy(buffer, "Done", sizeof(buffer) - 1);
    write(socket_descriptor, buffer, sizeof(buffer));

    close(socket_descriptor);
    unlink(socket_file_pathname.c_str());
}

// Reads the file at given path and returns its first piece of content (used
// to read the interface files which should only have one piece of data)
std::string read_file(std::string filepath) {
    std::string contents = "";

    std::ifstream file;

    file.open(filepath);

    if (file.is_open())
    {
        file >> contents;

        file.close();
    } else {
        std::cout << "[ERR]: Unable to open " << filepath << ":" << std::endl;
        std::cout << strerror(errno) << std::endl;
    }

    return contents;
}

// Sends a given message to the network monitor through the socket
int write_message(std::string message)
{
    int bytes_written = 0;

    // Clear the buffer of existing data and then copy the message into it
    memset(buffer, '\0', sizeof(buffer));
    strncpy(buffer, message.c_str(), sizeof(buffer) - 1);

    // Send the buffer over the socket file using write()
    bytes_written = write(socket_descriptor, buffer, sizeof(buffer));

    return bytes_written;
}

// Receives any incoming message from the network monitor and returns it as a
// string
std::string read_message()
{
    std::string message;

    // Clear the buffer of any existing data and then read incoming data from
    // the socket file into it
    memset(buffer, '\0', sizeof(buffer));
    size_t bytes_recieved = read(socket_descriptor, buffer, sizeof(buffer));

    message = buffer;

    return message;
}

// Monitors the interface with given interface_name by reading data from its
// director in /sys and printing that data out
void monitor_interface(std::string interface_name)
{
    // This will hold the data from the interface during a monitor iteration
    struct interface_information
    {
        std::string operstate;
        std::string carrier_up_count;
        std::string carrier_down_count;
        std::string rx_bytes;
        std::string rx_dropped;
        std::string rx_errors;
        std::string rx_packets;
        std::string tx_bytes;
        std::string tx_dropped;
        std::string tx_errors;
        std::string tx_packets;
    } interface_info;

    // Loop conditional flag which is set to false if the interface goes down
    bool link_is_up = true;

    // Send the "Monitoring" message to the network monitor
    write_message("Monitoring");

    // As long as the interface is up...
    while (link_is_up && isRunning)
    {
        // Get operstate info
        std::string filepath = interface_directory + "/operstate";
        interface_info.operstate = read_file(filepath);

        // Get carrier_up_count info
        filepath = interface_directory + "/carrier_up_count";
        interface_info.carrier_up_count = read_file(filepath);

        // Get carrier_down_count info
        filepath = interface_directory + "/carrier_down_count";
        interface_info.carrier_down_count = read_file(filepath);

        // Get rx_bytes info
        filepath = interface_directory + "/statistics/rx_bytes";
        interface_info.rx_bytes = read_file(filepath);

        // Get rx_dropped info
        filepath = interface_directory + "/statistics/rx_dropped";
        interface_info.rx_dropped = read_file(filepath);

        // Get rx_errors info
        filepath = interface_directory + "/statistics/rx_errors";
        interface_info.rx_errors = read_file(filepath);

        // Get rx_packets info
        filepath = interface_directory + "/statistics/rx_packets";
        interface_info.rx_packets = read_file(filepath);

        // Get tx_bytes info
        filepath = interface_directory + "/statistics/tx_bytes";
        interface_info.tx_bytes = read_file(filepath);

        // Get tx_dropped info
        filepath = interface_directory + "/statistics/tx_dropped";
        interface_info.tx_dropped = read_file(filepath);

        // Get tx_errors info
        filepath = interface_directory + "/statistics/tx_errors";
        interface_info.tx_errors = read_file(filepath);

        // Get tx_packets info
        filepath = interface_directory + "/statistics/tx_packets";
        interface_info.tx_packets = read_file(filepath);

        // Print out all the information
        std::cout << std::endl << "Interface:" << interface_name
            << " state:" << interface_info.operstate
            << " up_count:" << interface_info.carrier_up_count
            << " down_count:" << interface_info.carrier_down_count << std::endl
            << "rx_bytes:" << interface_info.rx_bytes
            << " rx_dropped:" << interface_info.rx_dropped
            << " rx_errors:" << interface_info.rx_errors
            << " rx_packets:" << interface_info.rx_packets << std::endl
            << "tx_bytes:" << interface_info.tx_bytes
            << " tx_dropped:" << interface_info.tx_dropped
            << " tx_errors:" << interface_info.tx_errors
            << " tx_packets:" << interface_info.tx_packets << std::endl;

        // If the operstate of the interface is not "up" then the interface has
        // gone down and we need to break this monitoring loop
        if (interface_info.operstate.compare("up") != 0
                && interface_info.operstate.length() > 0) {
            link_is_up = false;
        }

        // Sleep for 1 second before looping again
        sleep(1);
    }

    // Report to the network monitor that the link has gone down (the only way
    // in which the above loop is broken unless the process is killed)
    write_message("Link Down");
}

int main(int argc, char *argv[])
{
    struct sigaction new_action;

    bool should_continue = true;

    // Populate the sigaction struct's members for the handler and the mask
    new_action.sa_handler = signalHandler;
    sigemptyset(&(new_action.sa_mask));
    sigaddset(&(new_action.sa_mask), SIGINT);

    // Link the SIGINT signal to our signal handler
    int return_value = sigaction(SIGINT, &new_action, NULL);

    // If the signal has been linked successfully, we can continue
    if (return_value != -1) {
        // Make a connection to the server
        socket_descriptor = make_connection();

        // If the connection has been established, we can continue
        if (socket_descriptor != -1)
        {
            // Let the network monitor know we're ready to monitor
            write_message("Ready");

            // Grab the interface name specified as an argument and construct
            // it's directory path
            std::string interface_name = argv[1];
            interface_directory = interface_directory + interface_name;

            while (isRunning)
            {
                // Read any message available from the network monitor
                std::string message = read_message();

                // If the message is "Monitor", we need to start monitoring the
                // interface
                if (message.compare("Monitor") == 0)
                {
                    monitor_interface(interface_name);
                } 
                // If the message is "Set Link Up", we attempt to set the
                // interface status to up using an ioctl call
                else if (message.compare("Set Link Up") == 0)
                {
                    struct ifreq interface;

                    // Create a socket in order to target the interface
                    int interface_socket = socket(AF_INET, SOCK_DGRAM, 0);

                    if (interface_socket != -1)
                    {
                        memset(&interface, 0, sizeof(ifreq));

                        // Put the interface name and the up status flag in the
                        // interface information
                        strncpy(interface.ifr_name, interface_name.c_str(), IFNAMSIZ);
                        interface.ifr_flags |= IFF_UP;

                        // Use an ioctl call to send this interface flag
                        // to the interface specified using the name
                        int return_code = ioctl(interface_socket, SIOCSIFFLAGS, &interface);

                        // If we were unsuccessful, we print out the error
                        if (return_code == -1)
                        {
                            std::cout << "[ERR]: Unable to set interface up:" << std::endl
                                    << strerror(errno) << std::endl;
                        // Otherwise, we let the network monitor know that the
                        // interface is now up and ready to be monitored again
                        } else {
                            write_message("Link Up");
                        }
                    }
                }
                // If the message is "Shut Down" we clean up any open connection
                // and stop the main conditional loop by setting isRunning to
                // false
                else if (message.compare("Shut Down") == 0)
                {
                    clean_up();

                    isRunning = false;
                }
            }
        }
    } else {
        return return_value;
    }

    return 0;
}

// The signal handler (only used for SIGINT)
static void signalHandler(int signal) {
    // Check which signal is being used
    switch (signal) {
        // If we recieve a SIGINT signal, clean up any open connections and
        // stop the main conditional loop
        case SIGINT:
        {
            isRunning = false;
            clean_up();
            break;
        }
        default:
        {
            std::cout << "intfMonitor: undefined signal" << std::endl;
        }
    }
}
