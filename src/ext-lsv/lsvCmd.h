/**CFile****************************************************************

  FileName    [.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName []

  Synopsis    [External declarations.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: .h,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/
 
#ifndef EXT_LSV_h
#define EXT_LSV_h



////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include <string>
#include <fstream>
#include <vector>
#include <climits>
#include <iostream>
#include "base/abc/abc.h"
// #include <opt/dar/dar.h>
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"
#include "sat/cnf/cnf.h"

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////



ABC_NAMESPACE_HEADER_START


////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== lsvPa1.cpp ==========================================================*/
extern int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
extern int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
extern int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
/*=== lsvPa2.cpp ==========================================================*/
extern int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
extern int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);
/*=== lsvUtils.cpp ==========================================================*/
extern void printBits(unsigned int num, int size=32);

ABC_NAMESPACE_HEADER_END



#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////

