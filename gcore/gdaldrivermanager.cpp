/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * gdaldrivermanager.cpp
 *
 * The GDALDriverManager class from gdal_priv.h.
 * 
 * $Log$
 * Revision 1.23  2005/04/04 15:24:48  fwarmerdam
 * Most C entry points now CPL_STDCALL
 *
 * Revision 1.22  2005/01/19 20:27:17  fwarmerdam
 * Search in lib/gdalplugins instead of directly in lib directory.
 * Only try files endning in .dll, .so or .dylib.  see bug 746
 *
 * Revision 1.21  2004/11/22 16:01:29  fwarmerdam
 * added GDAL_PREFIX
 *
 * Revision 1.20  2004/09/23 16:21:32  fwarmerdam
 * added call to OSRCleanup()
 *
 * Revision 1.19  2004/04/29 19:14:27  warmerda
 * Avoid extra newline when autoregistering.
 *
 * Revision 1.18  2004/04/16 06:23:46  warmerda
 * Use CPLGetConfigOption() instead of getenv().
 *
 * Revision 1.17  2003/12/28 17:27:36  warmerda
 * added call to CPLFreeConfig()
 *
 * Revision 1.16  2003/05/20 19:10:36  warmerda
 * added GDAL_DATA and CPLGetConfigOptions support
 *
 * Revision 1.15  2003/04/30 17:13:48  warmerda
 * added docs for many C functions
 *
 * Revision 1.14  2002/12/03 04:41:41  warmerda
 * call CPLFinderClean() to cleanup memory
 *
 * Revision 1.13  2002/10/21 18:05:42  warmerda
 * added AutoSkipDrivers() method on driver manager
 *
 * Revision 1.12  2002/09/04 06:52:35  warmerda
 * added GDALDestroyDriverManager
 *
 * Revision 1.11  2002/07/09 20:33:12  warmerda
 * expand tabs
 *
 * Revision 1.10  2002/06/12 21:13:27  warmerda
 * use metadata based driver info
 *
 * Revision 1.9  2002/05/14 21:38:32  warmerda
 * make INST_DATA overidable with binary patch
 *
 * Revision 1.8  2001/07/18 04:04:30  warmerda
 * added CPL_CVSID
 *
 * Revision 1.7  2000/08/29 21:09:15  warmerda
 * added logic to push INST_DATA on data file search stack
 *
 * Revision 1.6  2000/04/22 12:25:41  warmerda
 * Documented AutoLoadDrivers().
 *
 * Revision 1.5  2000/04/04 23:44:29  warmerda
 * added AutoLoadDrivers() to GDALDriverManager
 *
 * Revision 1.4  2000/03/06 02:21:39  warmerda
 * added GDALRegisterDriver func
 *
 * Revision 1.3  1999/10/21 13:24:08  warmerda
 * Added documentation and C callable functions.
 *
 * Revision 1.2  1998/12/31 18:53:33  warmerda
 * Add GDALGetDriverByName
 *
 * Revision 1.1  1998/12/03 18:32:01  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

static char *pszUpdatableINST_DATA = 
"__INST_DATA_TARGET:                                                                                                                                      ";

/************************************************************************/
/* ==================================================================== */
/*                           GDALDriverManager                          */
/* ==================================================================== */
/************************************************************************/

static GDALDriverManager        *poDM = NULL;

/************************************************************************/
/*                        GetGDALDriverManager()                        */
/*                                                                      */
/*      A freestanding function to get the only instance of the         */
/*      GDALDriverManager.                                              */
/************************************************************************/

/**
 * Fetch the global GDAL driver manager.
 *
 * This function fetches the pointer to the singleton global driver manager.
 * If the driver manager doesn't exist it is automatically created.
 *
 * @return pointer to the global driver manager.  This should not be able
 * to fail.
 */

GDALDriverManager * GetGDALDriverManager()

{
    if( poDM == NULL )
        new GDALDriverManager();

    return( poDM );
}

/************************************************************************/
/*                         GDALDriverManager()                          */
/************************************************************************/

GDALDriverManager::GDALDriverManager()

{
    nDrivers = 0;
    papoDrivers = NULL;
    pszHome = CPLStrdup("");

    CPLAssert( poDM == NULL );
    poDM = this;

/* -------------------------------------------------------------------- */
/*      We want to push a location to search for data files             */
/*      supporting GDAL/OGR such as EPSG csv files, S-57 definition     */
/*      files, and so forth.  The static pszUpdateableINST_DATA         */
/*      string can be updated within the shared library or              */
/*      executable during an install to point installed data            */
/*      directory.  If it isn't burned in here then we use the          */
/*      INST_DATA macro (setup at configure time) if                    */
/*      available. Otherwise we don't push anything and we hope         */
/*      other mechanisms such as environment variables will have        */
/*      been employed.                                                  */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "GDAL_DATA", NULL ) != NULL )
    {
        CPLPushFinderLocation( CPLGetConfigOption( "GDAL_DATA", NULL ) );
    }
    else if( pszUpdatableINST_DATA[19] != ' ' )
    {
        CPLPushFinderLocation( pszUpdatableINST_DATA + 19 );
    }
    else
    {
#ifdef INST_DATA
        CPLPushFinderLocation( INST_DATA );
#endif
    }
}

/************************************************************************/
/*                         ~GDALDriverManager()                         */
/*                                                                      */
/*      Eventually this should also likely clean up all open            */
/*      datasets.  Or perhaps the drivers that own them should do       */
/*      that in their destructor?                                       */
/************************************************************************/

GDALDriverManager::~GDALDriverManager()

{
/* -------------------------------------------------------------------- */
/*      Destroy the existing drivers.                                   */
/* -------------------------------------------------------------------- */
    while( GetDriverCount() > 0 )
    {
        GDALDriver      *poDriver = GetDriver(0);

        DeregisterDriver(poDriver);
        delete poDriver;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup local memory.                                           */
/* -------------------------------------------------------------------- */
    VSIFree( papoDrivers );
    VSIFree( pszHome );

/* -------------------------------------------------------------------- */
/*      Blow away all the finder hints paths.  We really shouldn't      */
/*      be doing all of them, but it is currently hard to keep track    */
/*      of those that actually belong to us.                            */
/* -------------------------------------------------------------------- */
    CPLFinderClean();
    CPLFreeConfig();

/* -------------------------------------------------------------------- */
/*      Cleanup any memory allocated by the OGRSpatialReference         */
/*      related subsystem.                                              */
/* -------------------------------------------------------------------- */
    OSRCleanup();

/* -------------------------------------------------------------------- */
/*      Ensure the global driver manager pointer is NULLed out.         */
/* -------------------------------------------------------------------- */
    if( poDM == this )
        poDM = NULL;
}

/************************************************************************/
/*                           GetDriverCount()                           */
/************************************************************************/

/**
 * Fetch the number of registered drivers.
 *
 * This C analog to this is GDALGetDriverCount().
 *
 * @return the number of registered drivers.
 */

int GDALDriverManager::GetDriverCount()

{
    return( nDrivers );
}

/************************************************************************/
/*                         GDALGetDriverCount()                         */
/************************************************************************/

/**
 * @see GDALDriverManager::GetDriverCount()
 */

int CPL_STDCALL GDALGetDriverCount()

{
    return GetGDALDriverManager()->GetDriverCount();
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

/**
 * Fetch driver by index.
 *
 * This C analog to this is GDALGetDriver().
 *
 * @param iDriver the driver index from 0 to GetDriverCount()-1.
 *
 * @return the number of registered drivers.
 */

GDALDriver * GDALDriverManager::GetDriver( int iDriver )

{
    if( iDriver < 0 || iDriver >= nDrivers )
        return NULL;
    else
        return papoDrivers[iDriver];
}

/************************************************************************/
/*                           GDALGetDriver()                            */
/************************************************************************/

/**
 * @see GDALDriverManager::GetDriver()
 */

GDALDriverH CPL_STDCALL GDALGetDriver( int iDriver )

{
    return (GDALDriverH) GetGDALDriverManager()->GetDriver(iDriver);
}

/************************************************************************/
/*                           RegisterDriver()                           */
/************************************************************************/

/**
 * Register a driver for use.
 *
 * The C analog is GDALRegisterDriver().
 *
 * Normally this method is used by format specific C callable registration
 * entry points such as GDALRegister_GTiff() rather than being called
 * directly by application level code.
 *
 * If this driver (based on the object pointer, not short name) is already
 * registered, then no change is made, and the index of the existing driver
 * is returned.  Otherwise the driver list is extended, and the new driver
 * is added at the end.
 *
 * @param poDriver the driver to register.
 *
 * @return the index of the new installed driver.
 */

int GDALDriverManager::RegisterDriver( GDALDriver * poDriver )

{
/* -------------------------------------------------------------------- */
/*      If it is already registered, just return the existing           */
/*      index.                                                          */
/* -------------------------------------------------------------------- */
    if( GetDriverByName( poDriver->GetDescription() ) != NULL )
    {
        int             i;

        for( i = 0; i < nDrivers; i++ )
        {
            if( papoDrivers[i] == poDriver )
                return i;
        }

        CPLAssert( FALSE );
    }
    
/* -------------------------------------------------------------------- */
/*      Otherwise grow the list to hold the new entry.                  */
/* -------------------------------------------------------------------- */
    papoDrivers = (GDALDriver **)
        VSIRealloc(papoDrivers, sizeof(GDALDriver *) * (nDrivers+1));

    papoDrivers[nDrivers] = poDriver;
    nDrivers++;

    if( poDriver->pfnCreate != NULL )
        poDriver->SetMetadataItem( GDAL_DCAP_CREATE, "YES" );
    
    if( poDriver->pfnCreateCopy != NULL )
        poDriver->SetMetadataItem( GDAL_DCAP_CREATECOPY, "YES" );

    return( nDrivers - 1 );
}

/************************************************************************/
/*                         GDALRegisterDriver()                         */
/************************************************************************/

/**
 * @see GDALDriverManager::GetRegisterDriver()
 */

int CPL_STDCALL GDALRegisterDriver( GDALDriverH hDriver )

{
    return GetGDALDriverManager()->RegisterDriver( (GDALDriver *) hDriver );
}


/************************************************************************/
/*                          DeregisterDriver()                          */
/************************************************************************/

/**
 * Deregister the passed driver.
 *
 * If the driver isn't found no change is made.
 *
 * The C analog is GDALDeregisterDriver().
 *
 * @param poDriver the driver to deregister.
 */

void GDALDriverManager::DeregisterDriver( GDALDriver * poDriver )

{
    int         i;

    for( i = 0; i < nDrivers; i++ )
    {
        if( papoDrivers[i] == poDriver )
            break;
    }

    if( i == nDrivers )
        return;

    while( i < nDrivers-1 )
    {
        papoDrivers[i] = papoDrivers[i+1];
        i++;
    }
    nDrivers--;
}

/************************************************************************/
/*                        GDALDeregisterDriver()                        */
/************************************************************************/

/**
 * @see GDALDriverManager::GetDeregisterDriver()
 */

void CPL_STDCALL GDALDeregisterDriver( GDALDriverH hDriver )

{
    GetGDALDriverManager()->DeregisterDriver( (GDALDriver *) hDriver );
}


/************************************************************************/
/*                          GetDriverByName()                           */
/************************************************************************/

/**
 * Fetch a driver based on the short name.
 *
 * The C analog is the GDALGetDriverByName() function.
 *
 * @param pszName the short name, such as GTiff, being searched for.
 *
 * @return the identified driver, or NULL if no match is found.
 */

GDALDriver * GDALDriverManager::GetDriverByName( const char * pszName )

{
    int         i;

    for( i = 0; i < nDrivers; i++ )
    {
        if( EQUAL(papoDrivers[i]->GetDescription(), pszName) )
            return papoDrivers[i];
    }

    return NULL;
}

/************************************************************************/
/*                        GDALGetDriverByName()                         */
/************************************************************************/

/**
 * @see GDALDriverManager::GetDriverByName()
 */

GDALDriverH CPL_STDCALL GDALGetDriverByName( const char * pszName )

{
    return( GetGDALDriverManager()->GetDriverByName( pszName ) );
}

/************************************************************************/
/*                              GetHome()                               */
/************************************************************************/

const char *GDALDriverManager::GetHome()

{
    return pszHome;
}

/************************************************************************/
/*                              SetHome()                               */
/************************************************************************/

void GDALDriverManager::SetHome( const char * pszNewHome )

{
    CPLFree( pszHome );
    pszHome = CPLStrdup(pszNewHome);
}

/************************************************************************/
/*                          AutoSkipDrivers()                           */
/************************************************************************/

/**
 * This method unload undesirable drivers.
 *
 * All drivers specified in the space delimited list in the GDAL_SKIP 
 * environmentvariable) will be deregistered and destroyed.  This method 
 * should normally be called after registration of standard drivers to allow 
 * the user a way of unloading undesired drivers.  The GDALAllRegister()
 * function already invokes AutoSkipDrivers() at the end, so if that functions
 * is called, it should not be necessary to call this method from application
 * code. 
 */

void GDALDriverManager::AutoSkipDrivers()

{
    if( CPLGetConfigOption( "GDAL_SKIP", NULL ) == NULL )
        return;

    char **papszList = CSLTokenizeString( CPLGetConfigOption("GDAL_SKIP","") );

    for( int i = 0; i < CSLCount(papszList); i++ )
    {
        GDALDriver *poDriver = GetDriverByName( papszList[i] );

        if( poDriver == NULL )
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Unable to find driver %s to unload from GDAL_SKIP environment variable.", 
                      papszList[i] );
        else
        {
            CPLDebug( "GDAL", "AutoSkipDriver(%s)", papszList[i] );
            DeregisterDriver( poDriver );
            delete poDriver;
        }
            
    }

    CSLDestroy( papszList );
}

/************************************************************************/
/*                          AutoLoadDrivers()                           */
/************************************************************************/

/**
 * Auto-load GDAL drivers from shared libraries.
 *
 * This function will automatically load drivers from shared libraries.  It
 * searches the "driver path" for .so (or .dll) files that start with the
 * prefix "gdal_X.so".  It then tries to load them and then tries to call
 * a function within them called GDALRegister_X() where the 'X' is the same 
 * as the remainder of the shared library basename, or failing that to 
 * call GDALRegisterMe().  
 *
 * There are a few rules for the driver path.  If the GDAL_DRIVER_PATH
 * environment variable it set, it is taken to be a list of directories
 * to search separated by colons on unix, or semi-colons on Windows.  Otherwise
 * the /usr/local/lib/gdalplugins directory, and (if known) the lib/gdalplugins
 * subdirectory of the gdal home directory are searched. 
 */

void GDALDriverManager::AutoLoadDrivers()

{
    char     **papszSearchPath = NULL;
    const char *pszGDAL_DRIVER_PATH = 
        CPLGetConfigOption( "GDAL_DRIVER_PATH", NULL );

/* -------------------------------------------------------------------- */
/*      Where should we look for stuff?                                 */
/* -------------------------------------------------------------------- */
    if( pszGDAL_DRIVER_PATH != NULL )
    {
#ifdef WIN32
        papszSearchPath = 
            CSLTokenizeStringComplex( pszGDAL_DRIVER_PATH, ";", TRUE, FALSE );
#else
        papszSearchPath = 
            CSLTokenizeStringComplex( pszGDAL_DRIVER_PATH, ":", TRUE, FALSE );
#endif
    }
    else
    {
#ifdef GDAL_PREFIX
        papszSearchPath = CSLAddString( papszSearchPath, 
                                        GDAL_PREFIX "/lib/gdalplugins" );
#else
        papszSearchPath = CSLAddString( papszSearchPath, 
                                        "/usr/local/lib/gdalplugins" );
#endif

        if( strlen(GetHome()) > 0 )
        {
            papszSearchPath = CSLAddString( papszSearchPath, 
                                  CPLFormFilename( GetHome(), 
                                                   "lib/gdalplugins", NULL ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Scan each directory looking for files starting with gdal_       */
/* -------------------------------------------------------------------- */
    for( int iDir = 0; iDir < CSLCount(papszSearchPath); iDir++ )
    {
        char  **papszFiles = CPLReadDir( papszSearchPath[iDir] );

        for( int iFile = 0; iFile < CSLCount(papszFiles); iFile++ )
        {
            char   *pszFuncName;
            const char *pszFilename;
            const char *pszExtension = CPLGetExtension( papszFiles[iFile] );
            void   *pRegister;

            if( !EQUALN(papszFiles[iFile],"gdal_",5) )
                continue;

            if( !EQUAL(pszExtension,"dll") 
                && !EQUAL(pszExtension,"so") 
                && !EQUAL(pszExtension,"dylib") )
                continue;

            pszFuncName = (char *) CPLCalloc(strlen(papszFiles[iFile])+20,1);
            sprintf( pszFuncName, "GDALRegister_%s", 
                     CPLGetBasename(papszFiles[iFile]) + 5 );
            
            pszFilename = 
                CPLFormFilename( papszSearchPath[iDir], 
                                 papszFiles[iFile], NULL );

            pRegister = CPLGetSymbol( pszFilename, pszFuncName );
            if( pRegister == NULL )
            {
                strcpy( pszFuncName, "GDALRegisterMe" );
                pRegister = CPLGetSymbol( pszFilename, pszFuncName );
            }
            
            if( pRegister != NULL )
            {
                CPLDebug( "GDAL", "Auto register %s using %s.", 
                          pszFilename, pszFuncName );

                ((void (*)()) pRegister)();
            }

            CPLFree( pszFuncName );
        }

        CSLDestroy( papszFiles );
    }

    CSLDestroy( papszSearchPath );
}

/************************************************************************/
/*                      GDALDestroyDriverManager()                      */
/************************************************************************/

/**
 * Destroy the driver manager.
 *
 * Incidently unloads all managed drivers.
 */

void CPL_STDCALL GDALDestroyDriverManager( void )

{
    if( poDM != NULL )
        delete poDM;
}
