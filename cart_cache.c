////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_cache.c
//  Description    : This is the implementation of the cache for the CART
//                   driver.
//
//  Author         : Huaxin Li
//  Last Modified  : 11/27/16
//

// Includes
#include <stdlib.h>
#include <string.h>

// Project includes
#include "cart_cache.h"
#include "cmpsc311_log.h"

// Defines
int cacheSize = DEFAULT_CART_FRAME_CACHE_SIZE*2;
int current;        //current size of cache
struct elem{
    int memCart;
    int memFrm;
    int time;
    char memContent[CART_FRAME_SIZE];
};
struct elem *cache;

// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : set_cart_cache_size
// Description  : Set the size of the cache (must be called before init)
//
// Inputs       : max_frames - the maximum number of items your cache can hold
// Outputs      : 0 if successful, -1 if failure

int set_cart_cache_size(uint32_t max_frames) {
    cacheSize = max_frames;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_cart_cache
// Description  : Initialize the cache and note maximum frames
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int init_cart_cache(void) {
    
    cache = (struct elem *) calloc(cacheSize, sizeof(struct elem));
    for (int i = 0; i < cacheSize; i++) {
        cache[i].memCart = -1;
        cache[i].memFrm = -1;
        cache[i].time = 0;
    }
    current = 0;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : close_cart_cache
// Description  : Clear all of the contents of the cache, cleanup
//
// Inputs       : none
// Outputs      : o if successful, -1 if failure

int close_cart_cache(void) {
    
    for (int i = 0; i < cacheSize; i++) {
        cache[i].memCart = -1;
        cache[i].memFrm = -1;
        cache[i].time = 0;
        memset(cache[i].memContent, '\0', sizeof(cache[i].memContent));
    }
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : put_cart_cache
// Description  : Put an object into the frame cache
//
// Inputs       : cart - the cartridge number of the frame to cache
//                frm - the frame number of the frame to cache
//                buf - the buffer to insert into the cache
// Outputs      : 0 if successful, -1 if failure

int put_cart_cache(CartridgeIndex cart, CartFrameIndex frm, void *buf)  {
    
    for (int i = 0; i < current; i++)
        cache[i].time++;
    
    //if already in the cache, update
    for (int i = 0; i < cacheSize; i++) {
        if (cache[i].memCart == cart && cache[i].memFrm == frm) {
            memcpy(cache[i].memContent, buf, CART_FRAME_SIZE);
            cache[i].time = 0;
            return 0;
        }
    }
    
    //else insert
    if (current == cacheSize) {         //if full, find LRU, replace
        int max = cache[0].time;
        int maxIndex = 0;
        for (int i = 1; i < cacheSize; i++){
            if (max < cache[i].time){
                max = cache[i].time;
                maxIndex = i;
            }
        }
        cache[maxIndex].memCart = cart;
        cache[maxIndex].memFrm = frm;
        cache[maxIndex].time = 0;
        memcpy(cache[maxIndex].memContent, buf, CART_FRAME_SIZE);
        
        
    }else{                              //not full, just insert
        
        cache[current].memCart = cart;
        cache[current].memFrm = frm;
        memcpy(cache[current].memContent, buf, CART_FRAME_SIZE);
        current++;
    }
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_cart_cache
// Description  : Get an frame from the cache (and return it)
//
// Inputs       : cart - the cartridge number of the cartridge to find
//                frm - the number of the frame to find
// Outputs      : pointer to cached frame or NULL if not found

void * get_cart_cache(CartridgeIndex cart, CartFrameIndex frm) {
    
    for (int i = 0; i < current; i++)
        cache[i].time++;
    
    for (int i = 0; i < current; i++) {
        
        if (cache[i].memCart == cart && cache[i].memFrm == frm) {
            cache[i].time = 0;
            return cache[i].memContent;
        }
    }
    
    return NULL;
}


//
// Unit test

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cartCacheUnitTest
// Description  : Run a UNIT test checking the cache implementation
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int cartCacheUnitTest(void) {
    set_cart_cache_size(50);
    init_cart_cache();
    
    for (int i=0; i<cacheSize; i++) {
        logMessage(LOG_OUTPUT_LEVEL, "1-> %s,2->%d,3->%d, %d", cache[i].memContent,cache[i].memCart,cache[i].memFrm,cache[i].time);
        
    }
    char a[50] = "anddddddddddddd";
    put_cart_cache(0, 0, a);
    
    char *aget = get_cart_cache(0, 0);
    logMessage(LOG_OUTPUT_LEVEL, "get: %s", aget);
    
    int acom = strcmp(a, aget);
    logMessage(LOG_OUTPUT_LEVEL, "equal: %d", acom);
    
    char b[50] = "xxxxxxxxxxxxxx";
    put_cart_cache(0, 1, b);
    
    char *bget = get_cart_cache(0, 1);
    logMessage(LOG_OUTPUT_LEVEL, "get: %s", bget);
    
    int bcom = strcmp(b, bget);
    logMessage(LOG_OUTPUT_LEVEL, "equal: %d", bcom);
    
    char c[50] = "tttttttttttttt";
    put_cart_cache(0, 1, c);
    
    char *cget = get_cart_cache(0, 1);
    logMessage(LOG_OUTPUT_LEVEL, "get: %s", cget);
    
    int ccom = strcmp(c, cget);
    logMessage(LOG_OUTPUT_LEVEL, "equal: %d", ccom);
    
    for (int i=0; i<current; i++) {
        logMessage(LOG_OUTPUT_LEVEL, "1-> %s,2->%d,3->%d, %d", cache[i].memContent,cache[i].memCart,cache[i].memFrm,cache[i].time);
        
    }
    
    close_cart_cache();
    
    // Return successfully
    logMessage(LOG_OUTPUT_LEVEL, "Cache unit test completed successfully.");
    return(0);
}




