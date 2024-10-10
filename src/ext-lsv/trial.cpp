#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/aig/aig.h"
// Forward declaration of the command function
static int Hello_world(Abc_Frame_t* pAbc, int argc, char** argv);

// Command to print "Hello World"
int Hello_world(Abc_Frame_t* pAbc, int argc, char** argv) {
    printf("Hello World\n");
    return 0;
}

// Function to register the hello command
void registerHelloCommand(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_hello", Hello_world, 0);
}

/*
ABC data frame work
To access the data structure, you can typically use the function Abc_FrameGetGlobalFrame() provided by ABC, which returns a pointer to the global frame.
typedef struct Abc_Frame_t_ {
    Abc_Ntk_t* pNtkCur;           // current network
    Gia_Man_t* pGia;              // current AIG manager
    void* pManGlob;               // global manager
    void* pLibLut;                // LUT library
    void* pLibGen;                // generic gate library
    Vec_Str_t* pAbcTemp;          // temporary buffer
    int fBatchMode;               // batch mode flag
    Vec_Ptr_t* pAbcCommands;      // list of ABC commands
    Abc_Cex_t* pCex;              // counterexample
    void* pHistory;               // command history
    double timeFrame;             // time for the current frame
    // other fields...
} Abc_Frame_t;

*/