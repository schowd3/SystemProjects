/* Do Not Modify This File */

#ifndef CONSTANTS_H
#define CONSTANTS_H

/* Bit Flags Constants */
/* Note: These only pertain to the upper 6-bits when shifted */
#define TERMINATED      0x1
#define DEFUNCT         0x2
#define STOPPED         0x4
#define READY           0x8
#define CREATED         0x10
#define SUDO            0x20

#define EXITCODE_BITS   26

/* Simulation Constants */
#define MAX_COMMAND     255   /* Max Process Command Length */ 
#define MAX_LINE_LEN    512   /* Max Trace File Line Length */

#define LOWEST_PRIORITY  139  /* This is the highest value (lowest priority) */
#define HIGHEST_PRIORITY 0    /* This is the lowest value (highest priority) */

#endif /* CONSTANTS_H */
