////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the CART storage system.
//
//  Author         : Huaxin Li
//  Last Modified  : 11/23/16
//

// Includes
#include <stdlib.h>
#include <string.h>
// Project Includes
#include "cart_driver.h"
#include "cart_controller.h"
#include "cmpsc311_log.h"
#include "cart_cache.h"
#include "cart_network.h"

// Implementation
uint64_t ky1, ky2, rt1, ct1, fm1;

//a file is a struct containing many attributes
struct cartFile{
    char* fName;
    int fLength;
    int isOpen;
    int16_t fHandle;
    uint32_t pos;
    int fCart[CART_CARTRIDGE_SIZE];
    int fFrame[CART_CARTRIDGE_SIZE];
};

struct cartFile allFile[CART_MAX_TOTAL_FILES];
int fileCount = 0;
int currentFrame = -1;
int currentCart = 0;


////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_cart_opcode
// Description  : combine the registers to create a 64-bit opcode
//
// Inputs       : cky1 register
//                cky2 register
//                crt2 register
//                cct1 register
//                cfm1 register
// Outputs      : a 64-bit opcode

CartXferRegister create_cart_opcode
(uint64_t cky1, uint64_t cky2, uint64_t crt1, uint64_t cct1, uint64_t cfm1)
{
    CartXferRegister opCode;
    cky1 = (cky1 & 0xff) << 56;
    cky2 = (cky2 & 0xff) << 48;
    crt1 = (crt1 & 1) << 47;
    cct1 = (cct1 & 0xffff) << 31;
    cfm1 = (cfm1 & 0xffff) << 15;
    opCode = cky1 | cky2 | crt1 | cct1 | cfm1;
    return opCode;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_cart_opcode
// Description  : extract the registers from a 64-bit opcode
//
// Inputs      : a 64-bit opcode
// Outputs     : cky1 registers
//               cky2 registers
//               crt2 registers
//               cct1 registers
//               cfm1 registers

int32_t extract_cart_opcode
(CartXferRegister resp, uint64_t* eky1, uint64_t* eky2, uint64_t* ert1, uint64_t* ect1, uint64_t* efm1)
{
    *efm1 = (resp >> 15) & 0xffff;
    *ect1 = (resp >> 31) & 0xffff;
    *ert1 = (resp >> 47) & 1;
    *eky2 = (resp >> 48) & 0xff;
    *eky1 = (resp >> 56) & 0xff;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweron
// Description  : Startup up the CART interface, initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweron(void) {
    uint64_t initms;
    uint64_t ldcart;
    uint64_t bzero;
    
    //initialize
    if ((initms = create_cart_opcode(CART_OP_INITMS, 0, 0, 0, 0)) == -1) {
        logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail on init (cons)");
        return(-1);
    }
    CartXferRegister oinitms = client_cart_bus_request(initms, NULL);
    if (extract_cart_opcode(oinitms, &ky1, &ky2, &rt1, &ct1, &fm1)) {
        logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail on init (decon).");
        return(-1);
    }
    if (rt1) {
        logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail on init (return).");
        return(-1);
    }
    
    for (int i = 0; i < CART_MAX_CARTRIDGES; i++) {
        //load cart
        if ((ldcart = create_cart_opcode(CART_OP_LDCART, 0, 0, i, 0)) == -1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (cons)");
            return(-1);
        }
        CartXferRegister oldcart = client_cart_bus_request(ldcart, NULL);
        if (extract_cart_opcode(oldcart, &ky1, &ky2, &rt1, &ct1, &fm1)) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (decon).");
            return(-1);
        }
        if (rt1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (return).");
            return(-1);
        }
        
        //zero memory
        if ((bzero = create_cart_opcode(CART_OP_BZERO, 0, 0, 0, 0)) == -1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to zero memory (cons)");
            return(-1);
        }
        CartXferRegister obzero = client_cart_bus_request(bzero, NULL);
        if (extract_cart_opcode(obzero, &ky1, &ky2, &rt1, &ct1, &fm1)) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to zero memory (decon).");
            return(-1);
        }
        if (rt1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to zero memory (return).");
            return(-1);
        }
        
    }
    
    init_cart_cache();
    
    // Return successfully
    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweroff
// Description  : Shut down the CART interface, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweroff(void) {
    uint64_t ldcart;
    uint64_t bzero;
    uint64_t powoff;
    
    for (int i = 0; i < CART_MAX_CARTRIDGES; i++) {
        //load cart
        if ((ldcart = create_cart_opcode(CART_OP_LDCART, 0, 0, i, 0)) == -1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (cons)");
            return(-1);
        }
        CartXferRegister oldcart = client_cart_bus_request(ldcart, NULL);
        if (extract_cart_opcode(oldcart, &ky1, &ky2, &rt1, &ct1, &fm1)) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (decon).");
            return(-1);
        }
        if (rt1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (return).");
            return(-1);
        }
        
        //zero memory
        if ((bzero = create_cart_opcode(CART_OP_BZERO, 0, 0, 0, 0)) == -1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to zero memory (cons)");
            return(-1);
        }
        CartXferRegister obzero = client_cart_bus_request(bzero, NULL);
        if (extract_cart_opcode(obzero, &ky1, &ky2, &rt1, &ct1, &fm1)) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to zero memory (decon).");
            return(-1);
        }
        if (rt1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to zero memory (return).");
            return(-1);
        }
        
    }
    
    //close all open files
    for (int i = 0; i < fileCount; i++)
        if (allFile[i].isOpen == 1)
            cart_close(allFile[i].fHandle);
    
    //power off
    if ((powoff = create_cart_opcode(CART_OP_POWOFF, 0, 0, 0, 0)) == -1) {
        logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to power off (cons)");
        return(-1);
    }
    CartXferRegister opowoff = client_cart_bus_request(powoff, NULL);
    if (extract_cart_opcode(opowoff, &ky1, &ky2, &rt1, &ct1, &fm1)) {
        logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to power off (decon).");
        return(-1);
    }
    if (rt1) {
        logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to power off (return).");
        return(-1);
    }
    
    close_cart_cache();
    // Return successfully
    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t cart_open(char *path) {
    for (int i = 0; i < fileCount; i++) {
        if(strcmp(allFile[i].fName, path) == 0){  //if the file exist
            if (allFile[i].isOpen == 1) {         //if the file is already open
                logMessage(LOG_ERROR_LEVEL, "file already open.");
                return -1;
            }
            allFile[i].pos = 0;
            allFile[i].isOpen = 1;
            return allFile[i].fHandle;
        }
    }
    
    //if not exist
    allFile[fileCount].fName = path;
    allFile[fileCount].fHandle = fileCount;
    allFile[fileCount].fLength = 0;
    allFile[fileCount].pos = 0;
    allFile[fileCount].isOpen = 1;
    fileCount++;
    currentFrame++;
    if (currentFrame == 1024) {
        currentCart++;
        currentFrame = 0;
    }
    return allFile[fileCount - 1].fHandle;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t cart_close(int16_t fd) {
    if (fd > fileCount || fd < 0) {
        logMessage(LOG_ERROR_LEVEL, "Invalid file Handle.");
        return -1;
    }
    if (allFile[fd].isOpen == 0) {
        logMessage(LOG_ERROR_LEVEL, "file not open.");
        return -1;
    }
    allFile[fd].isOpen = 0;
    
    // Return successfully
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_read
// Description  : Reads "count" bytes from the file handle "fh" into the
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t cart_read(int16_t fd, void *buf, int32_t count) {
    if (fd > fileCount || fd < 0) {
        logMessage(LOG_ERROR_LEVEL, "Invalid file Handle.");
        return -1;
    }
    if (allFile[fd].isOpen == 0) {
        logMessage(LOG_ERROR_LEVEL, "file not open.");
        return -1;
    }
    if (count < 0) {
        logMessage(LOG_ERROR_LEVEL, "Invalid length");
        return -1;
    }
    
    uint64_t ldcart;
    uint64_t rdfrme;
    char tmp[CART_FRAME_SIZE];
    int originPos = allFile[fd].pos;
    int posFrame = originPos / CART_FRAME_SIZE;                         //record which frame the pos is in
    int lenFrame = allFile[fd].fLength / CART_FRAME_SIZE;               //record which frame the length is in
    int countFrame = (originPos + count) / CART_FRAME_SIZE;             //record which frame the count plus the pos is in
    int remains = CART_FRAME_SIZE - originPos % CART_FRAME_SIZE;        //remaining space of the original position frame
    
    for (int i = posFrame, j = 0; i <= lenFrame; i++, j++) {
        
        if (get_cart_cache(allFile[fd].fCart[i], allFile[fd].fFrame[i]) != NULL) {              //hit
            memcpy(tmp, get_cart_cache(allFile[fd].fCart[i], allFile[fd].fFrame[i]), CART_FRAME_SIZE);
        }else{                                                              //miss
            //load cart
            if ((ldcart = create_cart_opcode(CART_OP_LDCART, 0, 0, allFile[fd].fCart[i], 0)) == -1) {
                logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (cons)");
                return(-1);
            }
            CartXferRegister oldcart = client_cart_bus_request(ldcart, NULL);
            if (extract_cart_opcode(oldcart, &ky1, &ky2, &rt1, &ct1, &fm1)) {
                logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (decon).");
                return(-1);
            }
            if (rt1) {
                logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (return).");
                return(-1);
            }
            //read frame
            if ((rdfrme = create_cart_opcode(CART_OP_RDFRME, 0, 0, 0, allFile[fd].fFrame[i])) == -1) {
                logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to read frame (cons)");
                return(-1);
            }
            CartXferRegister ordfrme = client_cart_bus_request(rdfrme, tmp);
            if (extract_cart_opcode(ordfrme, &ky1, &ky2, &rt1, &ct1, &fm1)) {
                logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to read frame (decon).");
                return(-1);
            }
            if (rt1) {
                logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to read frame (return).");
                return(-1);
            }
            
            put_cart_cache(allFile[fd].fCart[i], allFile[fd].fFrame[i], tmp);           //if miss, copy frame to cache
        }
        
        if (i < lenFrame) {             //not reach length frame yet
            if (i == posFrame) {        //the first(pos) frame
                if (i == countFrame) {
                    memcpy(buf, tmp + originPos % CART_FRAME_SIZE, count);
                    allFile[fd].pos += count;
                    return count;
                }else{                  //if not the first frame, copy the remaing data from the first frame
                    memcpy(buf, tmp + originPos % CART_FRAME_SIZE, remains);
                    allFile[fd].pos += remains;
                    j--;
                }
            }else{
                if (i == countFrame) {  //the last(count) frame
                    memcpy(buf + remains + CART_FRAME_SIZE * j, tmp, (originPos + count) % CART_FRAME_SIZE);
                    allFile[fd].pos += (count + originPos) % CART_FRAME_SIZE;
                    return count;
                }else{                  //if not the last frame, copy the all data from that frame
                    memcpy(buf + remains + CART_FRAME_SIZE * j, tmp, CART_FRAME_SIZE);
                    allFile[fd].pos += CART_FRAME_SIZE;
                }
            }
            
        }else{                          //reach lenframe, compare length and count
            if (i == posFrame){
                if (originPos + count > allFile[fd].fLength){   //should read to the end of the file
                    memcpy(buf, tmp + originPos % CART_FRAME_SIZE, allFile[fd].fLength - originPos);
                    allFile[fd].pos += allFile[fd].fLength - originPos;
                    return allFile[fd].fLength - originPos;
                }else{                                          //enough bytes to read
                    memcpy(buf, tmp + originPos % CART_FRAME_SIZE, count);
                    allFile[fd].pos += count;
                    return count;
                }
            }else{
                if (originPos + count > allFile[fd].fLength){   //should read to the end of the file
                    memcpy(buf + remains + CART_FRAME_SIZE * j, tmp, allFile[fd].fLength % CART_FRAME_SIZE);
                    allFile[fd].pos += allFile[fd].fLength % CART_FRAME_SIZE;
                    return allFile[fd].fLength - originPos;
                }else{                                          //enough bytes to read
                    memcpy(buf + remains + CART_FRAME_SIZE * j, tmp, (originPos + count) % CART_FRAME_SIZE);
                    allFile[fd].pos += (count + originPos) % CART_FRAME_SIZE;
                    return count;
                }
            }
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_write
// Description  : Writes "count" bytes to the file handle "fh" from the
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t cart_write(int16_t fd, void *buf, int32_t count) {
    
    if (fd > fileCount || fd < 0) {
        logMessage(LOG_ERROR_LEVEL, "Invalid file Handle.");
        return -1;
    }
    if (allFile[fd].isOpen == 0) {
        logMessage(LOG_ERROR_LEVEL, "file not open.");
        return -1;
    }
    if (count < 0) {
        logMessage(LOG_ERROR_LEVEL, "Invalid length");
        return -1;
    }
    
    uint64_t ldcart;
    uint64_t rdfrme;
    uint64_t wrfrme;
    char tmp[CART_FRAME_SIZE];
    const int originPos = allFile[fd].pos;
    const int posFrame = originPos / CART_FRAME_SIZE;                   //record which frame the pos is in
    const int lenFrame = allFile[fd].fLength / CART_FRAME_SIZE;         //record which frame the len is in
    const int countFrame = (originPos + count) / CART_FRAME_SIZE;       //record which frame the count plus the pos is in
    int remains = CART_FRAME_SIZE - originPos % CART_FRAME_SIZE;        //remaining space of the original position frame
    
    //create a frame for a file's first write
    if (allFile[fd].pos == 0 && allFile[fd].fLength == 0) {
        allFile[fd].fFrame[0] = currentFrame;
        allFile[fd].fCart[0] = currentCart;
    }
    
    for (int i = posFrame, j = 0; i <= countFrame; i++, j++) {
         if (get_cart_cache(allFile[fd].fCart[i], allFile[fd].fFrame[i]) != NULL) {              //hit
                memcpy(tmp, get_cart_cache(allFile[fd].fCart[i], allFile[fd].fFrame[i]), CART_FRAME_SIZE);
            }else{                                                              //miss
                //load cart
                if ((ldcart = create_cart_opcode(CART_OP_LDCART, 0, 0, allFile[fd].fCart[i], 0)) == -1) {
                    logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (cons)");
                    return(-1);
                }
                CartXferRegister oldcart = client_cart_bus_request(ldcart, NULL);
                if (extract_cart_opcode(oldcart, &ky1, &ky2, &rt1, &ct1, &fm1)) {
                    logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (decon).");
                    return(-1);
                }
                if (rt1) {
                    logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to load cartridge (return).");
                    return(-1);
                }
                //read frame
                if ((rdfrme = create_cart_opcode(CART_OP_RDFRME, 0, 0, 0, allFile[fd].fFrame[i])) == -1) {
                    logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to read frame (cons)");
                    return(-1);
                }
                CartXferRegister ordfrme = client_cart_bus_request(rdfrme, tmp);
                if (extract_cart_opcode(ordfrme, &ky1, &ky2, &rt1, &ct1, &fm1)) {
                    logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to read frame (decon).");
                    return(-1);
                }
                if (rt1) {
                    logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to read frame (return).");
                    return(-1);
                }
            }
        if (i <= lenFrame) {                    //not reach length frame yet
            if (i == posFrame) {                            //the first(pos) frame
                if (i == countFrame) {
                    
                    memcpy(tmp + originPos % CART_FRAME_SIZE, buf, count);
                    allFile[fd].pos += count;
                    if (originPos + count > allFile[fd].fLength)
                        allFile[fd].fLength = originPos + count;
                    
                }else{                                      //if not the first frame, write the remaing data for the first frame
                    
                    memcpy(tmp + originPos % CART_FRAME_SIZE, buf, remains);
                    allFile[fd].pos += remains;
                    j--;
                }
            }else{                                          //the last(count) frame
                if (i == countFrame) {
                    memcpy(tmp, buf + remains + j * CART_FRAME_SIZE, (count + originPos) % CART_FRAME_SIZE);
                    allFile[fd].pos += (count + originPos) % CART_FRAME_SIZE;
                    if (originPos + count > allFile[fd].fLength)
                        allFile[fd].fLength = originPos + count;
                }else{                                      //if not the last frame, write to that frame until it is full
                    memcpy(tmp, buf + remains + j * CART_FRAME_SIZE, CART_FRAME_SIZE);
                    allFile[fd].pos += CART_FRAME_SIZE;
                }
            }
        } else {                                            //reach length frame
            
            currentFrame++;
            if (currentFrame == 1024) {
                currentCart++;
                currentFrame = 0;
            }
            allFile[fd].fFrame[i] = currentFrame;                      //create a new frame
            allFile[fd].fCart[i] = currentCart;
           
            if (i != countFrame) {                          //check if i reach the count's frame
                memcpy(tmp, buf + remains + j * CART_FRAME_SIZE, CART_FRAME_SIZE);
                allFile[fd].pos += CART_FRAME_SIZE;
            }else{
                memcpy(tmp, buf + remains + j * CART_FRAME_SIZE, (count + originPos) % CART_FRAME_SIZE);
                allFile[fd].pos += (count + originPos) % CART_FRAME_SIZE;
                allFile[fd].fLength = originPos + count;
            }
        }
        
        
        
        //write frame
        if ((wrfrme = create_cart_opcode(CART_OP_WRFRME, 0, 0, 0, allFile[fd].fFrame[i])) == -1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to write frame (cons)");
            return(-1);
        }
        CartXferRegister owrfrme = client_cart_bus_request(wrfrme, tmp);
        if (extract_cart_opcode(owrfrme, &ky1, &ky2, &rt1, &ct1, &fm1)) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to write frame (decon).");
            return(-1);
        }
        if (rt1) {
            logMessage(LOG_ERROR_LEVEL, "CART driver failed: fail to write frame (return).");
            return(-1);
        }
        put_cart_cache(allFile[fd].fCart[i], allFile[fd].fFrame[i], tmp);
        
        
    }
    return count;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t cart_seek(int16_t fd, uint32_t loc) {
    if (fd > fileCount || fd < 0) {
        logMessage(LOG_ERROR_LEVEL, "Invalid file Handle.");
        return -1;
    }
    if (loc > allFile[fd].fLength) {
        logMessage(LOG_ERROR_LEVEL, "loc is beyond the end of the file");
        return -1;
    }
    if (allFile[fd].isOpen == 0) {
        logMessage(LOG_ERROR_LEVEL, "file not open.");
        return -1;
    }
    allFile[fd].pos = loc;  //change the current position to loc
    
    // Return successfully
    return (0);
}


