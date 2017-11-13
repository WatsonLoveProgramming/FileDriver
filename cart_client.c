////////////////////////////////////////////////////////////////////////////////
//
//  File          : cart_client.c
//  Description   : This is the client side of the CART communication protocol.
//
//   Author       : Huaxin Li
//  Last Modified : 12/07/16
//

// Include Files
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

// Project Include Files
#include "cart_network.h"
#include "cmpsc311_util.h"
#include "cmpsc311_log.h"

//
//  Global data
int                socket_fd;
int                client_socket = -1;
int                cart_network_shutdown = 0;   // Flag indicating shutdown
unsigned char     *cart_network_address = NULL; // Address of CART server
unsigned short     cart_network_port = 0;       // Port of CART serve
unsigned long      CartControllerLLevel = LOG_INFO_LEVEL; // Controller log level (global)
unsigned long      CartDriverLLevel = 0;     // Driver log level (global)
unsigned long      CartSimulatorLLevel = 0;  // Driver log level (global)

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_cart_bus_request
// Description  : This the client operation that sends a request to the CART
//                server process.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : reg - the request reqisters for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

CartXferRegister client_cart_bus_request(CartXferRegister reg, void *buf) {
    
    struct sockaddr_in caddr;
    char *ip = CART_DEFAULT_IP;
    caddr.sin_family = AF_INET;
    caddr.sin_port = htons(CART_DEFAULT_PORT);
    
    if (client_socket == -1) {
        if ( inet_aton(ip, &caddr.sin_addr) == 0 ) {         //Setup the address
            return( -1 );
        }
        
        socket_fd = socket(PF_INET, SOCK_STREAM, 0);        //Create the socket
        if (socket_fd == -1) {
            printf( "Error on socket creation \n" );
            return( -1 );
        }
                                                            //Create the connection
        if ( connect(socket_fd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1 ) {
            printf( "Error on socket connect \n");
            return( -1 );
        }
        client_socket = 1;
    }
    
    uint64_t ky1 = (reg >> 56) & 0xff;
    uint64_t value = htonll64(reg);
    
    if (ky1 == CART_OP_RDFRME) {             // read
        if ( write( socket_fd, &value, sizeof(value)) != sizeof(value) ) {
            printf( "Error writing network data\n");
            return( -1 );
        }
        if ( read( socket_fd, &value, sizeof(value)) != sizeof(value) ) {
            printf( "Error reading network data \n" );
            return( -1 );
        }
        if ( read( socket_fd, buf, CART_FRAME_SIZE) != CART_FRAME_SIZE ) {
            printf( "Error reading network data \n" );
            return( -1 );
        }
        
    }else if (ky1 == CART_OP_WRFRME) {             //write
        if ( write( socket_fd, &value, sizeof(value)) != sizeof(value) ) {
            printf( "Error writing network data\n");
            return( -1 );
        }
        if ( write( socket_fd, buf, CART_FRAME_SIZE) != CART_FRAME_SIZE ) {
            printf( "Error writing network data\n");
            return( -1 );
        }
        if ( read( socket_fd, &value, sizeof(value)) != sizeof(value) ) {
            printf( "Error reading network data \n" );
            return( -1 );
        }
        
    }else if (ky1 == CART_OP_POWOFF) {             //shutdown
        if ( write( socket_fd, &value, sizeof(value)) != sizeof(value) ) {
            printf( "Error writing network data\n");
            return( -1 );
        }
        if ( read( socket_fd, &value, sizeof(value)) != sizeof(value) ) {
            printf( "Error reading network data \n" );
            return( -1 );
        }
        client_socket = -1;
        close(socket_fd);
        
    }else{                                          //others
        if ( write( socket_fd, &value, sizeof(value)) != sizeof(value) ) {
            printf( "Error writing network data\n");
            return( -1 );
        }
        if ( read( socket_fd, &value, sizeof(value)) != sizeof(value) ) {
            printf( "Error reading network data \n" );
            return( -1 );
        }
    }
    
    return ntohll64(value);
}


