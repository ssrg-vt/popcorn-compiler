/*
 *	File:   database.c
 *
 *      Description:
 *              A database is a freeform ASCII file that contains
 *              data of various types attached to names.
 *              An example database is:
 *
 *              !h2.molecule.name single str
 *                      "Diatomic hydrogen"
 *              !h2.molecule.abstract array string
 *               "Nowhere, here, everywhere"
 *               "Never, now, forever"
 *               "No one, someone, everyone"
 *              !h2.atom.names array str
 *               "H1"
 *               "H2"
 *              !h2.atom.types array str
 *               "HH"
 *                "HH"
 *              !h2.bonds.atom1 array int
 *               1
 *              !h2.bonds.atom2 array int
 *               2
 *              !h2.bonds.order array int
 *               1
 *              !h2.atom.coordinates.x array double
 *               1.2
 *               2.3
 *                      etc etc etc.
 *
 *		Data can be read from the file using either RANDOM access
 *		or SEQUENTIAL access.
 *
 *		As a rough rule of thumb,
 *		RANDOM access is convenient for files
 *		that are smaller than ONE megabyte.  This is because
 *		when RANDOM access files are opened, an initial pass
 *		is made over the file to determine where each entry
 *		begins, and when the file is closed, the entire file
 *		is copied to garantee that every entry has a unique name.
 *
 *		SEQUENTIAL access is for LARGE files, like
 *		coordinate dumps.
 *
 *		RANDOM access files can be opened as SEQUENTIAL with
 *		no problems, and SEQUENTIAL access files can be opened
 *		as RANDOM access files as long as the programmer
 *		can garantee that every entry in the SEQUENTIAL file
 *		has a unique name.
 *
 *              Each new piece of data begins with a line that
 *              starts with a '!' followed by the variable name
 *              followed by the type information.
 *              The actual data appears on the lines following the !line
 *              up until the next line beginning with a '!'
 *              or the end of file.
 *
 *              Database entry names are combinations of any character
 *              EXCEPT spaces, names are NOT ALLOWED TO CONTAIN SPACES. 
 *              The caller has the ability to define a Prefix that
 *              is automatically attatched to each name to make implementation
 *              of libraries within the database structure easier.
 *              The TOTAL database name MUST NEVER be longer than
 *              can be contained in the String object.
 */

#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#if !defined(SPEC_WINDOWS)
#include	<unistd.h>
#endif
#include    <math.h>

#include        "database.h"

#ifndef	TRUE
#define	TRUE	1
#endif
#ifndef	FALSE
#define	FALSE	0
#endif

#define TOTALCOLUMNS    16              /* Total number of columns allowed */

#define ENTRYINTEGERSTR "int"
#define ENTRYDOUBLESTR  "dbl"
#define ENTRYSTRINGSTR  "str"
#define ENTRYARRAYSTR   "array"
#define ENTRYTABLESTR   "table"
#define ENTRYSINGLESTR  "single"


#define	MESSAGE(x)
/*  #define	MESSAGE(x)	printf x  */
#define	DFATAL(x)	printf x

/*
 *---------------------------------------------------
 *
 *	String routines
 *
 */
 
/*
 *      sDBRemoveSpaces
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Remove all spaces from sIn and place the results in sOut.
 *
 */

char*   sDBRemoveSpaces( sIn, sOut )
char*   sIn;
char*   sOut;
{
char*   sWrite;

                /* Copy everything except spaces */


    sWrite = sOut;
    while ( (*sIn)!= '\0' ) {
        if ( (*sIn)!=' ' ) (*sWrite++)=(*sIn);
        sIn++;
    }
    (*sWrite) = '\0';
    return(sOut);

}




/*
 *      sDBRemoveControlAndPadding
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Remove all control characters and all Padding spaces,
 *      spaces at the start and end of the string.
 *
 *TODO This might not be machine independant
 */

char*   sDBRemoveControlAndPadding( sRaw, sResult )
char*   sRaw;
char*   sResult;
{
char*   sCur;
char*   sResultCur;

                /* First skip over the initial spaces and control characters */


    sCur = sRaw;
    while ( (*sCur!='\0') && (*sCur<=' ') ) sCur++;
    if ( *sCur == '\0' ) {
        *sResult = '\0';
        goto DONE;
    }
    
                /* Now copy the rest, minus control characters into sResult */
    sResultCur = sResult;
    while ( *sCur!='\0' ) {
        if ( *sCur >= ' ' ) {
            *sResultCur = *sCur;
            sResultCur++;
        }
        sCur++;
    }
    *sResultCur = '\0';
    
                /* Now remove the trailing spaces */
    if ( strlen(sResult) > 0 ) {
        sResultCur--;
        while ( *sResultCur == ' ' ) sResultCur--;
        sResultCur++;
        *sResultCur = '\0';
    }
    
DONE:
    return(sResult);

}




/*
 *      sDBRemoveLeadingSpaces
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Just like it says, return a string that has the first
 *      spaces removed.
 */

char*   sDBRemoveLeadingSpaces( sLine )
char*   sLine;
{
char*   sTemp;



    sTemp = sLine;
    while ( (*sTemp==' ') && ( *sTemp!='\0' )) sTemp++;
    strcpy( sLine, sTemp );
    return(sLine);

}




/*
 *      sDBRemoveFirstString
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Copy everything in the string up until the first space into sHead.
 *      Remove everything (including the space) from the string sLine.
 */

void	sDBRemoveFirstString( sLine, sHead )
char*   sLine;
char*   sHead;
{
char*   sTemp;



    sTemp = sLine;
    while ( (*sTemp!=' ') && ( *sTemp!='\0' )) sTemp++;
    if ( *sTemp == '\0' ) {
        strcpy( sHead, sLine );
        *sLine = '\0';
        return;
    }
    *sTemp = '\0';
    strcpy( sHead, sLine );
    sTemp++;
    strcpy( sLine, sTemp );

}


/*
 *---------------------------------------------------
 *
 *	Dict routines
 *
 *	Store a list of names.
 */

typedef	struct	{
	char		*cPKey;
	void		*vPData;
	} DICT_ENTRYt;

typedef	struct	{
	int		iEntries;
	DICT_ENTRYt	*dePEntries;
	} DICTt;

typedef	DICTt	*DICT;

typedef	int	DICTLOOP;


/*
 *	dDictCreate
 *
 *	Create a DICT.
 */
DICT	dDictCreate()
{
DICT	dNew;

    dNew = (DICT)malloc(sizeof(DICTt));
    dNew->iEntries = 0;
    dNew->dePEntries = NULL;
    return(dNew);
}	



/*
 *	DictDestroy
 *
 *	Destroy the DICT.
 */
void	DictDestroy( dPDict )
DICT	*dPDict;
{
int	i;
DICT_ENTRYt	*dePCur;

    for ( i=0, dePCur = (*dPDict)->dePEntries;
		i<(*dPDict)->iEntries;
		i++, dePCur++ ) {
	free(dePCur->cPKey);
    }
    free((*dPDict)->dePEntries);
    free((*dPDict));
    *dPDict = NULL;
}





/* 
 *	DictAdd
 *
 *	Add an entry to the DICT.
 */
void	DictAdd( dDict, cPKey, vPData )
DICT	dDict;
char	*cPKey;
void	*vPData;
{
DICT_ENTRYt	*dePNew;

    if ( dDict->dePEntries == NULL ) {
	dDict->dePEntries = (DICT_ENTRYt*)malloc(sizeof(DICT_ENTRYt));
	dePNew = dDict->dePEntries;
    } else {
	dePNew = (DICT_ENTRYt*)realloc(
			dDict->dePEntries, sizeof(DICT_ENTRYt)*
						(dDict->iEntries+1) );
	dDict->dePEntries = dePNew;
	dePNew = dDict->dePEntries + dDict->iEntries;
    }
    dDict->iEntries++;
    dePNew->cPKey = (char*)malloc(strlen(cPKey)+1);
    strcpy( dePNew->cPKey, cPKey );
    dePNew->vPData = vPData;
}


/*
 *	vPDictFind
 *
 *	Find an entry in the DICT.
 */
void	*vPDictFind( dDict, cPKey )
DICT	dDict;
char	*cPKey;
{
int		i;
DICT_ENTRYt	*dePCur;

    if ( dDict->dePEntries == NULL ) return(NULL);
    for ( i=0,dePCur=dDict->dePEntries; 
		i<dDict->iEntries; 
		i++, dePCur++ ) {
	if ( strcmp( dePCur->cPKey, cPKey ) == 0 ) {
	    return(dePCur->vPData);
	}
    }
    return(NULL);
}

/*
 *	vPDictDelete
 *
 *	Find an entry in the DICT, and remove it.
 */
void	*vPDictDelete( dDict, cPKey )
DICT	dDict;
char	*cPKey;
{
int		i, j;
DICT_ENTRYt	*dePCur;
Bool		bGotIt;
DICT_ENTRYt	*dePNext;
void		*vPData;

    bGotIt = FALSE;
    if ( dDict->dePEntries == NULL ) return(NULL);
    for ( i=0,dePCur=dDict->dePEntries; 
		i<dDict->iEntries; 
		i++, dePCur++ ) {
	if ( strcmp( dePCur->cPKey, cPKey ) == 0 ) {
	    bGotIt = TRUE;
	    break;
	}
    }
    if ( !bGotIt ) return(NULL);
    vPData = dePCur->vPData;
    if ( i < dDict->iEntries-1 ) {
	dePNext = dePCur+1;
	for ( j=i;
		j<(dDict->iEntries-1);
		j++, dePNext++, dePCur++ ) {
	    *dePCur = *dePNext;
	}
	dDict->iEntries--;
	dePCur = (DICT_ENTRYt*)realloc( dDict->dePEntries,
			sizeof(DICT_ENTRYt)*(dDict->iEntries) );
    } else {
	free(dDict->dePEntries);
	dDict->dePEntries = NULL;
    }
    return(vPData);
}




/*
 *	ziDictKeyCompare
 *
 *	Compare two DICT_ENTRYt* keys.  Note void* and subsequent casts
 *      to avoid an "incompatible with prototype" compiler warning.
 */
int	ziDictKeyCompare( const void *dePA, const void *dePB )
{
    return(strcmp( (const char *)( ((DICT_ENTRYt *)dePA)->cPKey ),
		   (const char *)( ((DICT_ENTRYt *)dePB)->cPKey ) ) );
}


/*
 *	dlDictLoop
 *
 *	Sort and then start loop over the DICT.
 */
DICTLOOP	dlDictLoop( dDict )
DICT		dDict;
{

    qsort( dDict->dePEntries, dDict->iEntries, 
		sizeof(DICT_ENTRYt), ziDictKeyCompare );


    return(0);
}


/*
 *	vPDictNext
 *
 *	Return the next element in the DICT.
 */
void	*vPDictNext( dDict, dlPCur, cPPKey )
DICT		dDict;
DICTLOOP	*dlPCur;
char		**cPPKey;
{
int	iCur;

    iCur = *(int*)dlPCur; 
    (*dlPCur)++;
    if ( iCur >= dDict->iEntries ) return(NULL);
    *cPPKey = dDict->dePEntries[iCur].cPKey;
    return(dDict->dePEntries[iCur].vPData );

}



/*
 *-----------------------------------------------------------------
 *
 *       Private routines
 *
 */

int	GiDBLastError = DB_ERROR_NONE;


#define	DB_CHECK_ACCESS(db,a) {if (db->iAccessMode!=a) {\
DFATAL(( "The DATABASE has the wrong access mode." ));}}


/*
 *      sDataBaseName
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Prepare the name for usage as a database entry name,
 *      meaning, remove leading and tailing spaces, 
 *      convert to lowercase, and attach the Prefix.
 */

static  char*   sDataBaseName( db, sOld, sNew )
DATABASE        db;
char*           sOld;
char*           sNew;
{
int             iPrefixLen;
String          sTemp;

                /* If there is a Prefix string then set it up properly */


    strcpy( sNew, "" );
    iPrefixLen = strlen(db->saPrefixStack[db->iPrefix]);
    if ( iPrefixLen != 0 ) {
        strcpy( sNew, db->saPrefixStack[db->iPrefix] );
    }
   
                /* Catenate the old name to the prefix */ 
                /* and convert it all to lowercase */

    sDBRemoveControlAndPadding( sOld, sTemp );
    strcat( sNew, sTemp );

    return(sNew);

}



/*
 *      ReportError
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Display an error message and line number in the DATABASE
 *      file where the error occured.
 */

static  void    ReportError( db, sError )
DATABASE        db;
char*           sError;
{


    printf( "An error occured in line: %d\n", db->iCurrentLine );
    printf( "Message: %s\n", sError );


}





/*
 *      ConstructDataHeader
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Compose the header for a database entry.
 */

static  void    ConstructDataHeader( sLine, sName, iType )
char*           sLine;
char*           sName;
int             iType;
{


    strcpy( sLine, "!" );
    strcat( sLine, sName );
    strcat( sLine, " " );
    switch ( iType & ENTRYMODIFIER ) {
        case ENTRYSINGLE:
            strcat( sLine, ENTRYSINGLESTR );
            break;
        case ENTRYARRAY:
            strcat( sLine, ENTRYARRAYSTR );
            break;
            
                /* If it is a table then there is no type info YET! */
                
        case ENTRYTABLE:
            strcat( sLine, ENTRYTABLESTR );
            return;
    }
    strcat( sLine, " " );
    switch ( iType & ENTRYTYPE ) {
        case ENTRYINTEGER:
            strcat( sLine, ENTRYINTEGERSTR );
            break;
        case ENTRYDOUBLE:
            strcat( sLine, ENTRYDOUBLESTR );
            break;
        case ENTRYSTRING:
            strcat( sLine, ENTRYSTRINGSTR );
            break;
    }

}







/*
 *      AddColumnType
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Concatenate the columns type string and name string to the
 *      header line.
 */

static  void    AddColumnType( sLine, iType, sName )
char*           sLine;
int             iType;
char*           sName;
{
        /* Add an extra space to seperate columns */



    strcat( sLine, " " );
    switch ( iType ) {
        case ENTRYINTEGER:
            strcat( sLine, " " );
            strcat( sLine, ENTRYINTEGERSTR );
            break;
        case ENTRYDOUBLE:
            strcat( sLine, " " );
            strcat( sLine, ENTRYDOUBLESTR );
            break;
        case ENTRYSTRING:
            strcat( sLine, " " );
            strcat( sLine, ENTRYSTRINGSTR );
            break;
    }
    strcat( sLine, " " );
    strcat( sLine, sName );

}








/*
 *      WriteDataLine
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Write a line of data to the database file.
 */

static  void    WriteDataLine( db, sLine )
DATABASE        db;
char*           sLine;
{


    fprintf( db->fDataBase, "%s\n", sLine );

}







/*
 *      zbDBReadLine
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Read a line from the database file.
 *	Copy the line into the sLookAhead field.
 *
 *	Return FALSE if no data line was read,
 *	which occurs if end of file is hit or a new
 *	header line is hit.
 */

static  Bool	zbDBReadLine( db, sLine )
DATABASE        db;
char*           sLine;
{


    if ( !feof(db->fDataBase) ) {
	do {
	    sLine[0] = '\0';
	    fgets( sLine, MAXDATALINELEN, db->fDataBase );
	} while ( sLine[0] != '\0' && sLine[0] == '\n' );
	if ( sLine[0] == '\0' ) return(FALSE);
	strcpy( db->sLookAhead, sLine );
	return(TRUE);
    } else {
	return(FALSE);
    }


}



/*
 *      zbDBReadDataLine
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Read a line of data from the database file.
 *
 *	Return FALSE if no data line was read,
 *	which occurs if end of file is hit or a new
 *	header line is hit.
 */

static  Bool	zbDBReadDataLine( db, sLine )
DATABASE        db;
char*           sLine;
{


    if ( zbDBReadLine( db, sLine ) ) {
	if ( sLine[0] == '!' ) return(FALSE);
	return(TRUE);
    } else {
	return(FALSE);
    }


}







/*
 *      eEntryCreate
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Create a new ENTRY.
 *      And initialize it.
 *
 */

static  ENTRY   eEntryCreate( db, sName, iType, lOffset )
DATABASE        db;
char*           sName;
int             iType;
long            lOffset;
{
ENTRY   eEntry;


    eEntry = (ENTRY)malloc(sizeof(ENTRYt));
    
    eEntry->iType = iType; 
    strcpy( eEntry->sName, sName );
    eEntry->lFileOffset = lOffset;
    return(eEntry);

}





/*
 *	zbDBParseSimpleHeader
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *	Parse the simple part of the header line, the name and the type.
 *	Dont try to parse the table information.
 *
 */

Bool	zbDBParseSimpleHeader( db, sRawLine, cPName, iPType )
DATABASE	db;
char*		sRawLine;
char*		cPName;
int*		iPType;
{
char		sLine[MAXDATALINELEN];
int		iType;
String		sModifier, sType;



		/* If there is no header then just returned */
		/* This is used by SEQUENTIAL access files */
		/* with the lDBSeqCurPos and DBSeqGoto routines */
		
    if ( sRawLine[0] == '\0' ) return(TRUE);
   
    iType = 0; 
    if ( sRawLine[0] == '!' ) {

        sDBRemoveControlAndPadding( sRawLine, sLine );
        sscanf( sLine, "!%s %s %s", cPName, sModifier, sType );
            
                        /* Define the modifier of the object */

        if ( strcmp( sModifier, ENTRYSINGLESTR ) == 0 ) {
            iType = ENTRYSINGLE;
        } else if ( strcmp( sModifier, ENTRYARRAYSTR ) == 0 ) {
            iType = ENTRYARRAY;
        } else if ( strcmp( sModifier, ENTRYTABLESTR ) == 0 ) {

		/* If the entry is a table then there is no */
		/* type information */

            iType = ENTRYTABLE;
        } else {
	    ReportError( db, "Unknown modifier" );
	    return(FALSE);
	}
        
                /* Define the type of the object */
	if ( iType != ENTRYTABLE ) {
	    if ( strcmp( sType, ENTRYINTEGERSTR ) == 0 ) {
		iType |= ENTRYINTEGER;
	    } else if ( strcmp( sType, ENTRYDOUBLESTR ) == 0 ) {
		iType |= ENTRYDOUBLE;
	    } else if ( strcmp( sType, ENTRYSTRINGSTR ) == 0 ) {
		iType |= ENTRYSTRING;
	    } else {
		ReportError( db, "Unknown entry type" );
		return(FALSE);
	    }
	}

	*iPType = iType;

    } else {
	DFATAL(( "Tried to parse:%s: as a header",
		sRawLine ));
    }
    return(TRUE);


}



	    
/*
 *      bScanDataBase
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Scan through the database file searching for
 *      data headers, and create dictionary entries for them.
 *
 *	Return TRUE if everything went alright, otherwise FALSE.
 */

static  Bool    bScanDataBase( db )
DATABASE        db;
{
char            sRawLine[MAXDATALINELEN];
long            lOffset;
String		sName;
int             iLineCount, iType;
ENTRY           eEntry;


    db->dEntries = dDictCreate();
    iLineCount = 0;
    eEntry = NULL;

    fseek( db->fDataBase, 0, 0 );

        /* Read each line of the file for headers, whose first */
        /* character is a '!'                                  */

    while ( !feof(db->fDataBase) ) {

        lOffset = ftell(db->fDataBase);
        sRawLine[0] = '\0';

	if ( !zbDBReadLine( db, sRawLine ) ) break;

                /* If the first character is a bang then it is a */
                /* data header line */
        
        if ( sRawLine[0] == '!' ) {

	    if ( !zbDBParseSimpleHeader( db, sRawLine, sName, &iType ) ) {
		return(FALSE);
	    }

                /* If we just finished an entry then define its */
                /* length */
            if ( eEntry != NULL ) eEntry->iRows = iLineCount;

                /* Look up the entry in the dictionary          */
                /* If it doesn't exist (and it shouldn't, each  */
                /* entry in the database should be unique)      */
                /* then create a new one                        */ 

/* NEXT: */
	    eEntry = (ENTRY)vPDictFind( (DICT)db->dEntries, sName );
            if ( eEntry == NULL ) {    
                eEntry = eEntryCreate( db, sName, iType, lOffset );
		DictAdd( (DICT)db->dEntries, sName, (void *)eEntry );
            } else {

		printf( "WARNING: Nonunique entry in database: %s found\n",
				sName );

			/* Update the entry */

		eEntry->iType = iType;
		eEntry->lFileOffset = lOffset;
	    }
		
            iLineCount = 0;
        } else if ( sRawLine[0] == ' ' ) {
	    iLineCount++;
	} else {
	    return(FALSE);
	}
    }

        /* Define the length of the last entry */

    if ( eEntry != NULL ) eEntry->iRows = iLineCount;

    return(TRUE);

}





/*
 *      StripInteger
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Strip an integer from the line, returning the rest of the line.
 */

static  void    StripInteger( sLine, iPInt )
char*   sLine;
int*    iPInt;
{
String  sHead;



    sDBRemoveLeadingSpaces( sLine );
    sDBRemoveFirstString( sLine, sHead );
    sscanf( sHead, "%d", iPInt );

}



/*
 *      StripDouble
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Strip a double precision value from the front of the string.
 *      Return the rest of the string.
 */

static  void    StripDouble( sLine, dPDbl )
char*           sLine;
double*         dPDbl;
{
String  sHead;


    sDBRemoveLeadingSpaces( sLine );
    sDBRemoveFirstString( sLine, sHead );
    sscanf( sHead, "%lG", dPDbl );

}





/*
 *      sStripString
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Read the string from the next line of the file.
 *      The string must be bracketed by double quotes.
 *      Double quotes may be entered into the string by using
 *      double double quotes  eg: ""  == "
 */

static  char*   sStripString( sLine, sStr )
char*           sLine;
char*           sStr;
{
char            c;
char*           sCur;
char*           sStart;



    sCur = sLine;
    sStart = sStr;
    
        /* Find the first QUOTE character */

    while ( (*sCur) != '"' ) sCur++;
    sCur++;

        /* Copy the following characters to sStr */
    do {
        c = (*sCur);
        sCur++;
        if ( c=='"' ) {
            c = (*sCur);
            sCur++;
            if ( c!='"' ) break;
        }
        (*sStr++) = c;
    } while ( (*sCur) != '\0' );
    *sStr = '\0';
    strcpy( sLine, sCur );
    
    return(sStart);

}




/*
 *      ConcatInteger
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Concatenate an integer to the string.
 */

static  void    ConcatInteger( sLine, iPVal )
char*           sLine;
int*            iPVal;
{
String  sTemp;


    sprintf( sTemp, " %d", *iPVal );
    strcat( sLine, sTemp );

}



/*
 *      ConcatDouble
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Concat a double to the line.
 */

static  void    ConcatDouble( sLine, dPVal )
char*           sLine;
double*         dPVal;
{
String          sTemp;
double		dAbs;


#if 0
    if ( *dPVal > 10000000000.0 ) {
	DFATAL(( "Number: %lf is too big to write to DATABASE",
			*dPVal ));
    }
#endif

    dAbs = fabs(*dPVal);
    if ( dAbs == 0.0 ) {
	sprintf( sTemp, " 0.0" );
    } else if ( 1000.0 > dAbs && dAbs > 0.0001 ) {
	sprintf( sTemp, " %lf", *dPVal );
    } else {
	sprintf( sTemp, " %lE", *dPVal );
    }
    strcat( sLine, sTemp );    

}





/*
 *      ConcatString
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Concatenate the string to the line.
 *      Individual single quotes in the string will be doubled.
 */

static  void    ConcatString( sLine, sStr )
char*           sLine;
char*           sStr;
{
int             iPos;
String          sTemp;



    sprintf( sTemp, " %c", '"' );
    iPos = 2;
    while ( *sStr != '\0' ) {
        sTemp[iPos++] = *sStr;
        if ( *sStr == '"' ) sTemp[iPos++] = '"';
        sStr++;
    }
    sTemp[iPos++] = '"';
    sTemp[iPos++] = '\0';
    strcat( sLine, sTemp );

}





/*
 *      zbDBGetValue
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Read the value from the database.
 *      The data type is determined by what is in eEntry.
 */

static  Bool    zbDBGetValue( db, iType, iPLines, PBuffer, iBufferInc )
DATABASE        db;
int		iType;
int*		iPLines;
char*           PBuffer;
int             iBufferInc;
{
String          sLine;



    *iPLines = 0;
    switch ( iType & ENTRYMODIFIER ) {
        case ENTRYSINGLE:
            switch ( iType & ENTRYTYPE ) {
                case ENTRYINTEGER:
                    zbDBReadDataLine( db, sLine );
                    StripInteger( sLine, (int*)PBuffer );
                    zbDBReadDataLine( db, sLine );
                    break;
                case ENTRYDOUBLE:
                    zbDBReadDataLine( db, sLine );
                    StripDouble( sLine, (double*)PBuffer );
                    zbDBReadDataLine( db, sLine );
                    break;
                case ENTRYSTRING:
                    zbDBReadDataLine( db, sLine );
                    sStripString( sLine, (char*)PBuffer );
                    zbDBReadDataLine( db, sLine );
                    break;
                default:
                    DFATAL(( "Unknown value type: %d\n", iType ));
                    break;
            }
	    *iPLines = 1;
            break;
        case ENTRYARRAY:

            switch ( iType & ENTRYTYPE ) {
                case ENTRYINTEGER:
		    while ( zbDBReadDataLine( db, sLine ) ) {
			StripInteger( sLine, (int*)PBuffer );
			PBuffer += iBufferInc;
			(*iPLines)++;
		    }
                    break;
                case ENTRYDOUBLE:
		    while ( zbDBReadDataLine( db, sLine ) ) {
			StripDouble( sLine, (double*)PBuffer );
			PBuffer += iBufferInc;
			(*iPLines)++;
		    }
                    break;
                case ENTRYSTRING:
		    while ( zbDBReadDataLine( db, sLine ) ) {
			sStripString( sLine, (char*)PBuffer );
			PBuffer += iBufferInc;
			(*iPLines)++;
		    }
                    break;
            }
            break;
    }

    return(TRUE);

}





/*
 *      zPutValue
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Read the value from the file.
 *      The data type is determined by what is in eEntry.
 */

static  void    zPutValue( db, iType, iLines, PBuffer, iBufferInc )
DATABASE        db;
int		iType;
int		iLines;
char*            PBuffer;
int             iBufferInc;
{
int             i;
char            sLine[MAXDATALINELEN];



    strcpy( sLine, "" );
    switch ( iType & ENTRYMODIFIER ) {
        case ENTRYSINGLE:
            switch ( iType & ENTRYTYPE ) {
                case ENTRYINTEGER:
                    ConcatInteger( sLine, (int*)PBuffer );
                    WriteDataLine( db, sLine );
                    break;
                case ENTRYDOUBLE:
                    ConcatDouble( sLine, (double*)PBuffer );
                    WriteDataLine( db, sLine );
                    break;
                case ENTRYSTRING:
                    ConcatString( sLine, (char*)PBuffer );
                    WriteDataLine( db, sLine );
                    break;
                default:
                    DFATAL( ("Unknown value type: %d\n", iType) );
                    break;
            }
            break;
        case ENTRYARRAY:
            switch ( iType & ENTRYTYPE ) {
                case ENTRYINTEGER:
                    for ( i=0; i<iLines; i++ ) {
                        strcpy( sLine, "" );
                        ConcatInteger( sLine, (int*)PBuffer );
                        WriteDataLine( db, sLine );
                        PBuffer += iBufferInc;
                    }
                    break;
                case ENTRYDOUBLE:
                    for ( i=0; i<iLines; i++ ) {
                        strcpy( sLine, "" );
                        ConcatDouble( sLine, (double*)PBuffer );
                        WriteDataLine( db, sLine );
                        PBuffer += iBufferInc;
                    }
                    break;
                case ENTRYSTRING:
                    for ( i=0; i<iLines; i++ ) {
                        strcpy( sLine, "" );
                        ConcatString( sLine, (char*)PBuffer );
                        WriteDataLine( db, sLine );
                        PBuffer += iBufferInc;
                    }
                    break;
            }
            break;
    }

}








/*
 *      TransferEntryToNewFile
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Copy the entry into the new file and adjust the
 *      entry to the new position within the file
 */

static  void    TransferEntryToNewFile( eEntry, db, fNew )
ENTRY           eEntry;
DATABASE        db;
FILE            *fNew;
{
long    lNewOffset;
char    sLine[MAXDATALINELEN];
int     i;

                /* Look for the end of the new file */ 


    fseek( fNew, 0, 2 ); 
    lNewOffset = ftell(fNew);
                /* Copy the Entry from the old file to the new file */
                /* First the header and then everything following it */
                /* until EOF or reached !       */ 
    fseek( db->fDataBase, eEntry->lFileOffset, 0 );
    zbDBReadDataLine( db, sLine );
    fputs( sLine, fNew );
    for ( i=0; i<eEntry->iRows; i++ ) {
        zbDBReadDataLine( db, sLine );
        fputs( sLine, fNew );
    }

                /* Update the Entry, point it to the new offset in */
                /* the new file */

    eEntry->lFileOffset = lNewOffset;

}

         




/*
 *      CompactDataBase
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Compact the DataBase and return the updated database.
 *      Compaction updates the entries file offset, so it
 *      must go to completion, otherwise some entries will 
 *      have offsets in the old file and some in the new file.
 */

void    CompactDataBase( db )
DATABASE        db;
{
FILE            *fNew;
String          sNewName;
DICTLOOP        dlLoop;
ENTRY           eEntry;
char		*cPKey;

                /* Open a scratch file for compaction */
    strcpy( sNewName, "t9423848" );
    fNew = fopen( sNewName, "w" );
    if ( fNew == NULL ) ReportError( db, "Could not open scratch file" );

                /* Loop through the contents of the dictionary */
    dlLoop = dlDictLoop( (DICT)db->dEntries );
    while ( (eEntry = (ENTRY)vPDictNext( (DICT)db->dEntries, &dlLoop, &cPKey)) ) {
        TransferEntryToNewFile( eEntry, db, fNew );
    }

                /* Now close and delete the old file */
    fclose(db->fDataBase);
    unlink( db->sFileName );

                /* Rename the new file */
                /* and set it as the current database file */
    fclose(fNew);
    rename( sNewName, db->sFileName );
    db->fDataBase = fopen( db->sFileName, "r+" );
    db->bCompactFileAtClose = FALSE;
    
    MESSAGE(( "Compacting DONE!!!!!\n" ));

}




/*
 *      ePrepareDatabaseForEntry
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Prepare the database to accept a new piece of data.
 *      1) Seek to the end of the file.
 *      2) Check the database to see if there is already
 *              such an entry, if there is, update its type
 *              and position, if there isn't then create it.
 */

static  ENTRY   ePrepareDatabaseForEntry( db, sEntry, iType, iRows )
DATABASE        db;
char*           sEntry;
int             iType;
int             iRows;
{
long            lOffset;
ENTRY           eEntry;

        /* Find the current end of the file */


    fseek( db->fDataBase, 0, 2 );
    lOffset = ftell( db->fDataBase );

        /* Look up the entry, if it doesnt exist then create it */
    eEntry = (ENTRY)vPDictFind( (DICT)db->dEntries, sEntry );
    if ( eEntry == NULL ) {

                /* There is no entry like this so create a new one */
        eEntry = eEntryCreate( db, sEntry, iType, lOffset );
        eEntry->iRows = iRows;
        DictAdd( (DICT)db->dEntries, sEntry, (void *)eEntry );
	MESSAGE(( "Added NEW entry: %s\n", sEntry ));
    } else {
                /* Update the old entry */
        eEntry->iType   = iType;
        eEntry->iRows   = iRows;
        eEntry->lFileOffset = lOffset;

        db->bCompactFileAtClose = TRUE;
	MESSAGE(( "Updated existing entry: %s\n", sEntry ));
    }
    return(eEntry);

}

                        



/*
 *====================================================================
 *
 *      Public routines
 *
 */

/*
 *-----------------
 *
 *	Public routines for accessing DB_RANDOM_ACCESS files.
 */


/*
 *      dbDBRndOpen
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Open a DATABASE file, scan through the file, and setup the
 *      file offsets for each piece of data in the file.
 */

DATABASE        dbDBRndOpen( sFileName, iOpenMode )
char*           sFileName;
int		iOpenMode;
{
DATABASE        db;
Bool		bExists;
char		cFirst;

                /* Create the database and open the file */


    db = (DATABASE)malloc(sizeof(DATABASEt));

                /* If the file cannot be opened read then open it write */

    GiDBLastError = DB_ERROR_NONE;

    bExists = FALSE;
    switch( iOpenMode ) {
	case OPENREADONLY:
	    db->fDataBase = fopen( sFileName, "r" );
	    if ( !db->fDataBase ) {
		GiDBLastError = DB_ERROR_INVALID_FILE;
	    } else bExists = TRUE;
	    break;
	case OPENREADWRITE:
	    db->fDataBase = fopen( sFileName, "r+" );
	    if ( db->fDataBase == NULL ) {
		db->fDataBase = fopen( sFileName, "w+" );
		if ( !db->fDataBase ) {
		    GiDBLastError = DB_ERROR_INVALID_FILE;
		}
	    } else {
		bExists = TRUE;
	    }
	    break;
	default:
	    DFATAL(( "Illegal database open mode\n" ));
	    break;
    }

		/* If the file exists then check if the first character is a '!' */

    if ( bExists ) {
	fseek( db->fDataBase, 0, 0 );
	cFirst = fgetc( db->fDataBase );
	if ( cFirst != '!' ) {
	    GiDBLastError = DB_ERROR_INVALID_DATABASE;
	}
    }

    if ( GiDBLastError != DB_ERROR_NONE ) {
	free(db);
	return(NULL);
    }

    strcpy( db->sFileName, sFileName );
    db->iOpenMode = iOpenMode;
    db->iAccessMode = DB_RANDOM_ACCESS;

    DBZeroPrefix( db );
    db->bCompactFileAtClose = FALSE;

                /* Scan through the database to find where each */
                /* variable starts                              */
		/* If the scan is unsuccessful then report */
		/* that it is an invalid database */

    if ( !bScanDataBase(db) ) {
	GiDBLastError = DB_ERROR_INVALID_DATABASE;
	DBClose( &db );
	return(NULL);
    }
    return(db);

}



/*
 *      bDBRndDeleteEntry
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Search for an entry in the database, if it is found,
 *      delete it and return TRUE, otherwise return FALSE.
 *      The entry will be removed when the file is compressed at the end.
 *
 */

Bool    bDBRndDeleteEntry( db, sOrgEntry )
DATABASE                db;
char*           sOrgEntry;
{
ENTRY           eEntry;
String          sEntry;

	/* If the file is read-only then print an error */



    DB_CHECK_ACCESS( db, DB_RANDOM_ACCESS );

    if ( db->iOpenMode == OPENREADONLY ) {
	DFATAL(( "DATABASE is read-only!" ));
    }

    sDataBaseName( db, sOrgEntry, sEntry );

    eEntry = (ENTRY)vPDictFind( (DICT)db->dEntries, sEntry );
    if ( eEntry == NULL ) return(FALSE);
    vPDictDelete( (DICT)db->dEntries, sEntry );
    db->bCompactFileAtClose = TRUE;
    return(TRUE);

}



/*
 *	DBRndLoopEntryWithPrefix
 *
 *	Initialize a loop over all entries that have the 
 *	prefix.
 */

void	DBRndLoopEntryWithPrefix( db, sOrgEntry )
DATABASE	db;
char*		sOrgEntry;
{
String          sEntry;



    sDataBaseName( db, sOrgEntry, sEntry );
    strcpy( db->sLoopPrefix, sEntry );
    db->dlEntryLoop = dlDictLoop( (DICT)db->dEntries );


}




/*
 *	bDBRndNextEntryWithPrefix
 *
 *	Return the next entry with the required prefix.
 *	Return FALSE if there are no more.
 */

Bool	bDBRndNextEntryWithPrefix( db, sEntry )
DATABASE	db;
char*		sEntry;
{
int		iLen;
char		*cPKey;


    iLen = strlen(db->sLoopPrefix);
    while ( vPDictNext( (DICT)db->dEntries, &(db->dlEntryLoop), &cPKey )) {
	strcpy( sEntry, cPKey );
	if ( strncmp( sEntry, db->sLoopPrefix, iLen ) == 0 ) {
	    return(TRUE);
	}
    }
    return(FALSE);


}


    
/*
 *-----------------------------------
 *
 *	Public routines for accessing DB_SEQUENTIAL_ACCESS files.
 */



/*
 *      dbDBSeqOpen
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Open a DATABASE file in DB_SEQUENTIAL_ACCESS mode.
 */

DATABASE        dbDBSeqOpen( sFileName, iOpenMode )
char*           sFileName;
int		iOpenMode;
{
DATABASE        db;

                /* Create the database and open the file */


    db = (DATABASE)malloc(sizeof(DATABASEt));

                /* If the file cannot be opened read then open it write */

    switch( iOpenMode ) {
	case OPENREADONLY:
	    db->fDataBase = fopen( sFileName, "r" );
	    break;
	case OPENREADWRITE:
	    db->fDataBase = fopen( sFileName, "r+" );
	    if ( db->fDataBase == NULL ) {
		db->fDataBase = fopen( sFileName, "w+" );
	    }
	    break;
	default:
	    DFATAL(( "Illegal database open mode\n" ));
	    break;
    }

    if ( db->fDataBase == NULL ) return(NULL);

    strcpy( db->sFileName, sFileName );
    db->iOpenMode = iOpenMode;
    db->iAccessMode = DB_SEQUENTIAL_ACCESS;

    DBZeroPrefix( db );
    db->bCompactFileAtClose = FALSE;

		/* Rewind the database to the start of the file */
		/* and get it ready to read */

    DBSeqRewind( db );
    db->iLastSequentialOperation = DB_READ;
    return(db);

}





/*
 *	DBSeqRewind
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *	Rewind the DB_SEQUENTIAL_ACCESS database to the
 *	start of the file and ready it to read.
 */

void	DBSeqRewind( db )
DATABASE	db;
{



    DB_CHECK_ACCESS( db, DB_SEQUENTIAL_ACCESS );

		/* Rewind the file */

    fseek( db->fDataBase, 0, 0 );
    if ( !feof(db->fDataBase) ) {
	zbDBReadLine( db, db->sLookAhead );
    }


}





/*
 *	DBSeqSkipData
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *	Skip the current piece of data in a DB_SEQUENTIAL_ACCESS
 *	file.
 *	Skip until the next entry starts.
 */

void	DBSeqSkipData( db )
DATABASE		db;
{



    DB_CHECK_ACCESS( db, DB_SEQUENTIAL_ACCESS );

    while ( zbDBReadLine( db, db->sLookAhead ) ) ;


}




/*
 *	lDBSeqCurPos
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *	Return the current file position.
 *	This number can be passed to DBSeqGoto to jump
 *	to the start of an entry.
 */

long	lDBSeqCurPos( db )
DATABASE		db;
{
long		lPos;


    lPos = ftell(db->fDataBase);
    return(lPos);

}



/*
 *	DBSeqGoto
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *	Jump to the file position passed in (lPos).
 *	(lPos) should be a value that was returned by lDBSeqCurPos
 *	after a DBGetType call, or before any DBGetxxx call.
 */

void	DBSeqGoto( db, lPos )
DATABASE	db;
long		lPos;
{

    fseek( db->fDataBase, lPos, 0 );

	/* Note that there is no header to parse */
    db->sLookAhead[0] = '\0';

}




/*
 *-----------------------------------------
 *
 *	Public routines for accessing both
 *	DB_SEQUENTIAL_ACCESS and DB_RANDOM_ACCESS files.
 */





/*
 *      bDBGetType
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *	If the file is DB_RANDOM_ACCESS then
 *      Return type information about the ENTRY
 *      return the modifier/type in iType, and the length information
 *      in iLength.
 *      The modifier/type is stored in bit form in iType.  To determine
 *      the type, & iType with ENTRYINTEGER . . .
 *
 *	If the file is DB_SEQUENTIAL_ACCESS then
 *	the name of the entry will be returnED in sOrgEntry and
 *	the type will be returned in *iPType, *iPLength will
 *	be set to LENGTH_NOT_KNOWN.
 */

Bool    bDBGetType( db, sOrgEntry, iPType, iPLength )
DATABASE                db;
char*           sOrgEntry;
int*            iPType;
int*            iPLength;
{
ENTRY           eEntry;
String          sEntry;



    MESSAGE(( "Getting value type\n" ));

    if ( db->iAccessMode == DB_SEQUENTIAL_ACCESS ) {
	if ( db->iLastSequentialOperation != DB_READ ) {
	    DFATAL(( "Illegal read of sequential file after a write" ));
	}
	if ( !feof(db->fDataBase) ) {
	    zbDBParseSimpleHeader( db, db->sLookAhead, sOrgEntry, iPType );
	    *iPLength = LENGTH_NOT_KNOWN;
	    db->iLastSequentialOperation = DB_READ;
	    return(TRUE);
	} else return(FALSE);
    }

    sDataBaseName( db, sOrgEntry, sEntry );

    eEntry = (ENTRY)vPDictFind( (DICT)db->dEntries, sEntry );
    if ( eEntry == NULL ) return(FALSE);
    
    *iPType = eEntry->iType;
    *iPLength = eEntry->iRows;
    return(TRUE);

}







/*
 *      bDBGetValue
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Read the entry value into the buffer provided by the
 *      caller.
 *
 *	If the file is DB_SEQUENTIAL_ACCESS then
 *	return the name of the entry in sOrgEntry, and
 *	the number of lines in *iPLength.
 */

Bool    bDBGetValue( dbData, sOrgEntry, iPLength, PBuffer, iBufferInc )
DATABASE        dbData;
char*           sOrgEntry;
int*		iPLength;
char*            PBuffer;
int             iBufferInc;
{
ENTRY           eEntry;
char            sHeader[MAXDATALINELEN];
String          sEntry;
int		iType;



    MESSAGE(( "Getting value\n" ));

    if ( dbData->iAccessMode == DB_SEQUENTIAL_ACCESS ) {
	zbDBParseSimpleHeader( dbData, dbData->sLookAhead,
				sOrgEntry, &iType );
    } else {

	sDataBaseName( dbData, sOrgEntry, sEntry );

		/* Look up the entry in the DICT */

	eEntry = (ENTRY)vPDictFind( (DICT)dbData->dEntries, sEntry );
	if ( eEntry == NULL ) return(FALSE);
	iType = eEntry->iType;

		/* Seek to the entry in the file */

	fseek( dbData->fDataBase, eEntry->lFileOffset, 0 );
	zbDBReadLine( dbData, sHeader );
    }
    
        /* Read the data itself */

    if ( !zbDBGetValue( dbData, iType, iPLength, PBuffer, iBufferInc ) ) return(FALSE);

    return(TRUE);

}






/*
 *      DBPutValue
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Write a piece of data to the database.
 *      Write it to the current end of the file.
 */

void    DBPutValue( db, sOrgEntry, iType, iCount, PData, iDataInc )
DATABASE        db;
char*           sOrgEntry;
int             iType;
int             iCount;
char*            PData;
int             iDataInc;
{
String          sEntry;
ENTRY           eEntry;
char            sLine[MAXDATALINELEN];

	/* If the file is read-only then print an error */



    if ( db->iOpenMode == OPENREADONLY ) {
	DFATAL(( "DATABASE is read-only!" ));
    }

    if ( (iType & ENTRYMODIFIER) == 0 ) 
        DFATAL(( "When PUTing into a DATABASE there must be a MODIFIER!" )); 
    if ( (iType & ENTRYTYPE) == 0 ) 
        DFATAL(( "When PUTing into a DATABASE there must be a TYPE!" )); 

    sDataBaseName( db, sOrgEntry, sEntry );

		/* If the file is DB_SEQUENTIAL_ACCESS */
		/* then jump to the end of the file */

    if ( db->iAccessMode == DB_SEQUENTIAL_ACCESS ) {

		/* Jump to the end */
	fseek( db->fDataBase, 0, 2 );
    } else {
	eEntry = ePrepareDatabaseForEntry( db, sEntry, iType, iCount );
    }

                /* Output the header */

    ConstructDataHeader( sLine, sEntry, iType );
    fprintf( db->fDataBase, "%s\n", sLine );
    zPutValue( db, iType, iCount, PData, iDataInc );

    fflush( db->fDataBase );

}




/* Only compile the following if LINT is NOT defined */
/* This prevents lint from crashing on the functions */
/* with so many arguments                            */

#ifndef LINT

/*
 *      bDBGetTableType
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Return table information.
 *      Return the number of lines, the type, and the number and name
 *      of each column.  The first four sets of number/name are integer
 *      type, the second four sets are double type and the last four
 *      sets are string type.
 *
 *	If the file is a DB_SEQUENTIAL_ACCESS file then return
 *	the table type of the current header which is in db->sLookAhead.
 *	The name of the entry will be returnED in sOrgEntry and
 *	the type will be returned in *iPType.  *iPLength
 *	will be set to LENGTH_NOT_KNOWN.
 */

Bool    bDBGetTableType( db, sOrgEntry, iPType, iPLength,
                        iPInt1Column, sInt1Name,
                        iPInt2Column, sInt2Name,
                        iPInt3Column, sInt3Name,
                        iPInt4Column, sInt4Name,
                        iPInt5Column, sInt5Name,
                        iPInt6Column, sInt6Name,
                        iPInt7Column, sInt7Name,
                        iPInt8Column, sInt8Name,
                        iPDouble1Column, sDouble1Name,
                        iPDouble2Column, sDouble2Name,
                        iPDouble3Column, sDouble3Name,
                        iPDouble4Column, sDouble4Name,
                        iPString1Column, sString1Name,
                        iPString2Column, sString2Name,
                        iPString3Column, sString3Name,
                        iPString4Column, sString4Name,
                        iPString5Column, sString5Name )
DATABASE        db;
char*           sOrgEntry;
int*            iPType;
int*            iPLength;
int*            iPInt1Column;
char*           sInt1Name;
int*            iPInt2Column;
char*           sInt2Name;
int*            iPInt3Column;
char*           sInt3Name;
int*            iPInt4Column;
char*           sInt4Name;
int*            iPInt5Column;
char*           sInt5Name;
int*            iPInt6Column;
char*           sInt6Name;
int*            iPInt7Column;
char*           sInt7Name;
int*            iPInt8Column;
char*           sInt8Name;
int*            iPDouble1Column;
char*           sDouble1Name;
int*            iPDouble2Column;
char*           sDouble2Name;
int*            iPDouble3Column;
char*           sDouble3Name;
int*            iPDouble4Column;
char*           sDouble4Name;
int*            iPString1Column;
char*           sString1Name;
int*            iPString2Column;
char*           sString2Name;
int*            iPString3Column;
char*           sString3Name;
int*            iPString4Column;
char*           sString4Name;
int*            iPString5Column;
char*           sString5Name;
{
String          sEntry, sName, sType;
String		sTemp;
char            sLine[MAXDATALINELEN];
ENTRY           eEntry;
int             iIntCol, iDoubleCol, iStringCol;
int             iColumn, iType;



    MESSAGE(( "Getting table types\n" ));

		/* Copy the line to parse into sLine */

    if ( db->iAccessMode == DB_SEQUENTIAL_ACCESS ) {
	zbDBParseSimpleHeader( db, db->sLookAhead, sOrgEntry, &iType );
	*iPType = iType;
	*iPLength = LENGTH_NOT_KNOWN;
	strcpy( sLine, db->sLookAhead );

    } else {

	sDataBaseName( db, sOrgEntry, sEntry );

	eEntry = (ENTRY)vPDictFind( (DICT)db->dEntries, sEntry );
	if ( eEntry == NULL ) return(FALSE);

	iType = eEntry->iType;
	*iPType = eEntry->iType;
	*iPLength = eEntry->iRows;

	fseek( db->fDataBase, eEntry->lFileOffset, 0 );
	zbDBReadDataLine( db, sLine );
    }

                /* If the entry exists but is not a table then return TRUE */

    if ( (iType & ENTRYMODIFIER) != ENTRYTABLE ) return(TRUE);

    *iPInt1Column = 0;
    *iPInt2Column = 0;
    *iPInt3Column = 0;
    *iPInt4Column = 0;
    *iPInt5Column = 0;
    *iPInt6Column = 0;
    *iPInt7Column = 0;
    *iPInt8Column = 0;

    *iPDouble1Column = 0;
    *iPDouble2Column = 0;
    *iPDouble3Column = 0;
    *iPDouble4Column = 0;

    *iPString1Column = 0;
    *iPString2Column = 0;
    *iPString3Column = 0;
    *iPString4Column = 0;
    *iPString5Column = 0;

    iIntCol = 1;
    iDoubleCol = 1;
    iStringCol = 1;
    iColumn = 1;

                /* Read the header line and parse it */

    sDBRemoveLeadingSpaces( sLine );

        /* Remove the objects name */

    sDBRemoveFirstString( sLine, sName );

        /* Remove the objects type, (table) */

    sDBRemoveLeadingSpaces( sLine );
    sDBRemoveFirstString( sLine, sType );

    do {
        sDBRemoveLeadingSpaces( sLine );

                /* If there is no more stuff then stop */

        if ( strlen(sLine)==0 ) break;
        sDBRemoveFirstString( sLine, sTemp );
	sDBRemoveControlAndPadding( sTemp, sType );
        sDBRemoveLeadingSpaces( sLine );
        sDBRemoveFirstString( sLine, sTemp );
        sDBRemoveControlAndPadding( sTemp, sName );

        if ( strcmp( sType, ENTRYINTEGERSTR )==0 ) {
            switch ( iIntCol ) {
                case 1:
                    *iPInt1Column = iColumn;
                    strcpy( sInt1Name, sName );
                    break;
                case 2:
                    *iPInt2Column = iColumn;
                    strcpy( sInt2Name, sName );
                    break;
                case 3:
                    *iPInt3Column = iColumn;
                    strcpy( sInt3Name, sName );
                    break;
                case 4:
                    *iPInt4Column = iColumn;
                    strcpy( sInt4Name, sName );
                    break;
                case 5:
                    *iPInt5Column = iColumn;
                    strcpy( sInt5Name, sName );
                    break;
                case 6:
                    *iPInt6Column = iColumn;
                    strcpy( sInt6Name, sName );
                    break;
                case 7:
                    *iPInt7Column = iColumn;
                    strcpy( sInt7Name, sName );
                    break;
                case 8:
                    *iPInt8Column = iColumn;
                    strcpy( sInt8Name, sName );
                    break;
            }
            iIntCol++;
         } else if ( strcmp( sType, ENTRYDOUBLESTR )==0 ) {
            switch ( iDoubleCol ) {
                case 1:
                    *iPDouble1Column = iColumn;
                    strcpy( sDouble1Name, sName );
                    break;
                case 2:
                    *iPDouble2Column = iColumn;
                    strcpy( sDouble2Name, sName );
                    break;
                case 3:
                    *iPDouble3Column = iColumn;
                    strcpy( sDouble3Name, sName );
                    break;
                case 4:
                    *iPDouble4Column = iColumn;
                    strcpy( sDouble4Name, sName );
                    break;
            }
            iDoubleCol++;
         } else if ( strcmp( sType, ENTRYSTRINGSTR )==0 ) {
            switch ( iStringCol ) {
                case 1:
                    *iPString1Column = iColumn;
                    strcpy( sString1Name, sName );
                    break;
                case 2:
                    *iPString2Column = iColumn;
                    strcpy( sString2Name, sName );
                    break;
                case 3:
                    *iPString3Column = iColumn;
                    strcpy( sString3Name, sName );
                    break;
                case 4:
                    *iPString4Column = iColumn;
                    strcpy( sString4Name, sName );
                    break;
                case 5:
                    *iPString5Column = iColumn;
                    strcpy( sString5Name, sName );
                    break;
            }
            iStringCol++;
        } else {
            ReportError( db, "Illegal table type!" );
        }
        iColumn++;
    } while ( 1==1 );
    
    return(TRUE);

}









/*
 *      bDBGetTable
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Return the contents of a table.
 */
	/*VARARGS0*/

Bool    bDBGetTable( db, sOrgEntry, iPLength,
                        iInt1Column, PInt1, iInt1Skip,
                        iInt2Column, PInt2, iInt2Skip,
                        iInt3Column, PInt3, iInt3Skip,
                        iInt4Column, PInt4, iInt4Skip,
                        iInt5Column, PInt5, iInt5Skip,
                        iInt6Column, PInt6, iInt6Skip,
                        iInt7Column, PInt7, iInt7Skip,
                        iInt8Column, PInt8, iInt8Skip,
                        iDouble1Column, PDouble1, iDouble1Skip,
                        iDouble2Column, PDouble2, iDouble2Skip,
                        iDouble3Column, PDouble3, iDouble3Skip,
                        iDouble4Column, PDouble4, iDouble4Skip,
                        iString1Column, PString1, iString1Skip,
                        iString2Column, PString2, iString2Skip,
                        iString3Column, PString3, iString3Skip,
                        iString4Column, PString4, iString4Skip,
                        iString5Column, PString5, iString5Skip )
DATABASE        db;
char*		sOrgEntry;
int*            iPLength;
int             iInt1Column;
char*           PInt1;
int             iInt1Skip;
int             iInt2Column;
char*           PInt2;
int             iInt2Skip;
int             iInt3Column;
char*           PInt3;
int             iInt3Skip;
int             iInt4Column;
char*           PInt4;
int             iInt4Skip;
int             iInt5Column;
char*           PInt5;
int             iInt5Skip;
int             iInt6Column;
char*           PInt6;
int             iInt6Skip;
int             iInt7Column;
char*           PInt7;
int             iInt7Skip;
int             iInt8Column;
char*           PInt8;
int             iInt8Skip;
int             iDouble1Column;
char*           PDouble1;
int             iDouble1Skip;
int             iDouble2Column;
char*           PDouble2;
int             iDouble2Skip;
int             iDouble3Column;
char*           PDouble3;
int             iDouble3Skip;
int             iDouble4Column;
char*           PDouble4;
int             iDouble4Skip;
int             iString1Column;
char*           PString1;
int             iString1Skip;
int             iString2Column;
char*           PString2;
int             iString2Skip;
int             iString3Column;
char*           PString3;
int             iString3Skip;
int             iString4Column;
char*           PString4;
int             iString4Skip;
int             iString5Column;
char*           PString5;
int             iString5Skip;
{
String          sEntry, sName, sType;
char            sLine[MAXDATALINELEN];
ENTRY           eEntry;
int             iColumn, iType;



    MESSAGE(( "Getting table\n" ));

    *iPLength = 0;

		/* If the file is DB_SEQUENTIAL_ACCESS */
		/* then get the entries name */

    if ( db->iAccessMode == DB_SEQUENTIAL_ACCESS ) {
	zbDBParseSimpleHeader( db, db->sLookAhead,
				sOrgEntry, &iType );
    } else {

	sDataBaseName( db, sOrgEntry, sEntry );

	eEntry = (ENTRY)vPDictFind( (DICT)db->dEntries, sEntry );
	if ( eEntry == NULL ) return(FALSE);

                /* If the entry exists but is not a table then return FALSE */

	if ( (eEntry->iType & ENTRYMODIFIER) != ENTRYTABLE ) return(FALSE);

                /* Read the header line and parse it */

	fseek( db->fDataBase, eEntry->lFileOffset, 0 );
	zbDBReadDataLine( db, sLine );
	sDBRemoveLeadingSpaces( sLine );

	        /* Remove the objects name */

    	sDBRemoveFirstString( sLine, sName );

        	/* Remove the objects type, (table) */

    	sDBRemoveLeadingSpaces( sLine );
    	sDBRemoveFirstString( sLine, sType );

    }

                /* Read every line */

    while ( zbDBReadDataLine( db, sLine ) ) {
        iColumn = 1;
	(*iPLength)++;

        do {
                /* If there is no more stuff then stop */

            if ( iColumn == iInt1Column ) {
                StripInteger( sLine, (int*)PInt1 );
                (PInt1) += iInt1Skip;
            } else if ( iColumn == iInt2Column ) {
                StripInteger( sLine, (int*)PInt2 );
                (PInt2) += iInt2Skip;
            } else if ( iColumn == iInt3Column ) {
                StripInteger( sLine, (int*)PInt3 );
                (PInt3) += iInt3Skip;
            } else if ( iColumn == iInt4Column ) {
                StripInteger( sLine, (int*)PInt4 );
                (PInt4) += iInt4Skip;
            } else if ( iColumn == iInt5Column ) {
                StripInteger( sLine, (int*)PInt5 );
                (PInt5) += iInt5Skip;
            } else if ( iColumn == iInt6Column ) {
                StripInteger( sLine, (int*)PInt6 );
                (PInt6) += iInt6Skip;
            } else if ( iColumn == iInt7Column ) {
                StripInteger( sLine, (int*)PInt7 );
                (PInt7) += iInt7Skip;
            } else if ( iColumn == iInt8Column ) {
                StripInteger( sLine, (int*)PInt8 );
                (PInt8) += iInt8Skip;

            } else if ( iColumn == iDouble1Column ) {
                StripDouble( sLine, (double*)PDouble1 );
                (PDouble1) += iDouble1Skip;
            } else if ( iColumn == iDouble2Column ) {
                StripDouble( sLine, (double*)PDouble2 );
                (PDouble2) += iDouble2Skip;
            } else if ( iColumn == iDouble3Column ) {
                StripDouble( sLine, (double*)PDouble3 );
                (PDouble3) += iDouble3Skip;
            } else if ( iColumn == iDouble4Column ) {
                StripDouble( sLine, (double*)PDouble4 );
                (PDouble4) += iDouble4Skip;

            } else if ( iColumn == iString1Column ) {
                sStripString( sLine, PString1 );
                (PString1) += iString1Skip;
            } else if ( iColumn == iString2Column ) {
                sStripString( sLine, PString2 );
                (PString2) += iString2Skip;
            } else if ( iColumn == iString3Column ) {
                sStripString( sLine, PString3 );
                (PString3) += iString3Skip;
            } else if ( iColumn == iString4Column ) {
                sStripString( sLine, PString4 );
                (PString4) += iString4Skip;
            } else if ( iColumn == iString5Column ) {
                sStripString( sLine, PString5 );
                (PString5) += iString5Skip;
            }
            iColumn++;
        } while ( strlen(sLine) != 0 );
    }
    
    return(TRUE);

}






/*
 *      DBPutTable
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Write the contents of a table.
 */
	/*VARARGS0*/

void    DBPutTable( db, sOrgEntry, iLines,
                        iInt1Column, sInt1, PInt1, iInt1Skip,
                        iInt2Column, sInt2, PInt2, iInt2Skip,
                        iInt3Column, sInt3, PInt3, iInt3Skip,
                        iInt4Column, sInt4, PInt4, iInt4Skip,
                        iInt5Column, sInt5, PInt5, iInt5Skip,
                        iInt6Column, sInt6, PInt6, iInt6Skip,
                        iInt7Column, sInt7, PInt7, iInt7Skip,
                        iInt8Column, sInt8, PInt8, iInt8Skip,
                        iDouble1Column, sDouble1, PDouble1, iDouble1Skip,
                        iDouble2Column, sDouble2, PDouble2, iDouble2Skip,
                        iDouble3Column, sDouble3, PDouble3, iDouble3Skip,
                        iDouble4Column, sDouble4, PDouble4, iDouble4Skip,
                        iString1Column, sString1, PString1, iString1Skip,
                        iString2Column, sString2, PString2, iString2Skip,
                        iString3Column, sString3, PString3, iString3Skip,
                        iString4Column, sString4, PString4, iString4Skip,
                        iString5Column, sString5, PString5, iString5Skip )
DATABASE        db;
char*		sOrgEntry;
int             iLines;
int             iInt1Column;
char*           sInt1;
char*           PInt1;
int             iInt1Skip;
int             iInt2Column;
char*           sInt2;
char*           PInt2;
int             iInt2Skip;
int             iInt3Column;
char*           sInt3;
char*           PInt3;
int             iInt3Skip;
int             iInt4Column;
char*           sInt4;
char*           PInt4;
int             iInt4Skip;
int             iInt5Column;
char*           sInt5;
char*           PInt5;
int             iInt5Skip;
int             iInt6Column;
char*           sInt6;
char*           PInt6;
int             iInt6Skip;
int             iInt7Column;
char*           sInt7;
char*           PInt7;
int             iInt7Skip;
int             iInt8Column;
char*           sInt8;
char*           PInt8;
int             iInt8Skip;
int             iDouble1Column;
char*           sDouble1;
char*           PDouble1;
int             iDouble1Skip;
int             iDouble2Column;
char*           sDouble2;
char*           PDouble2;
int             iDouble2Skip;
int             iDouble3Column;
char*           sDouble3;
char*           PDouble3;
int             iDouble3Skip;
int             iDouble4Column;
char*           sDouble4;
char*           PDouble4;
int             iDouble4Skip;
int             iString1Column;
char*           sString1;
char*           PString1;
int             iString1Skip;
int             iString2Column;
char*           sString2;
char*           PString2;
int             iString2Skip;
int             iString3Column;
char*           sString3;
char*           PString3;
int             iString3Skip;
int             iString4Column;
char*           sString4;
char*           PString4;
int             iString4Skip;
int             iString5Column;
char*           sString5;
char*           PString5;
int             iString5Skip;
{
String          sEntry;
char            sLine[MAXDATALINELEN];
ENTRY           eEntry;
int             iColumn, i;
int		iError;

	/* If the file is read-only then print an error */



    if ( db->iOpenMode == OPENREADONLY ) {
	DFATAL(( "DATABASE is read-only!" ));
    }

    MESSAGE(( "Putting table\n" ));

    sDataBaseName( db, sOrgEntry, sEntry );

    if ( db->iAccessMode == DB_SEQUENTIAL_ACCESS ) {

		/* Jump to the end */
	iError = fseek( db->fDataBase, 0L, 2 );
	MESSAGE(( "fseek returned = %d\n", iError ));
    } else {
	eEntry = ePrepareDatabaseForEntry( db, sEntry, ENTRYTABLE, iLines );
    }


                /* Construct and output the header */

    ConstructDataHeader( sLine, sEntry, ENTRYTABLE );
    for ( i=1; i<=TOTALCOLUMNS; i++ ) {
        if ( i==iInt1Column ) AddColumnType( sLine, ENTRYINTEGER, sInt1 );
        else if ( i==iInt2Column ) AddColumnType( sLine, ENTRYINTEGER, sInt2 );
        else if ( i==iInt3Column ) AddColumnType( sLine, ENTRYINTEGER, sInt3 );
        else if ( i==iInt4Column ) AddColumnType( sLine, ENTRYINTEGER, sInt4 );
        else if ( i==iInt5Column ) AddColumnType( sLine, ENTRYINTEGER, sInt5 );
        else if ( i==iInt6Column ) AddColumnType( sLine, ENTRYINTEGER, sInt6 );
        else if ( i==iInt7Column ) AddColumnType( sLine, ENTRYINTEGER, sInt7 );
        else if ( i==iInt8Column ) AddColumnType( sLine, ENTRYINTEGER, sInt8 );
        else if ( i==iDouble1Column )
                 AddColumnType( sLine, ENTRYDOUBLE, sDouble1 );
        else if ( i==iDouble2Column )
                 AddColumnType( sLine, ENTRYDOUBLE, sDouble2 );
        else if ( i==iDouble3Column )
                 AddColumnType( sLine, ENTRYDOUBLE, sDouble3 );
        else if ( i==iDouble4Column )
                 AddColumnType( sLine, ENTRYDOUBLE, sDouble4 );
        else if ( i==iString1Column )
                 AddColumnType( sLine, ENTRYSTRING, sString1 );
        else if ( i==iString2Column )
                 AddColumnType( sLine, ENTRYSTRING, sString2 );
        else if ( i==iString3Column )
                 AddColumnType( sLine, ENTRYSTRING, sString3 );
        else if ( i==iString4Column )
                 AddColumnType( sLine, ENTRYSTRING, sString4 );
        else if ( i==iString5Column )
                 AddColumnType( sLine, ENTRYSTRING, sString5 );
    }

    WriteDataLine( db, sLine );

                /* Write the data */

    for ( i=0; i<iLines; i++ ) {

        strcpy( sLine, "" );

        for ( iColumn=1; iColumn<=TOTALCOLUMNS; iColumn++ ) {
        
                /* If there is no more stuff then stop */

            if ( iColumn == iInt1Column ) {
                ConcatInteger( sLine, (int *)PInt1 );
                (PInt1) += iInt1Skip;
            } else if ( iColumn == iInt2Column ) {
                ConcatInteger( sLine, (int *)PInt2 );
                (PInt2) += iInt2Skip;
            } else if ( iColumn == iInt3Column ) {
                ConcatInteger( sLine, (int *)PInt3 );
                (PInt3) += iInt3Skip;
            } else if ( iColumn == iInt4Column ) {
                ConcatInteger( sLine, (int *)PInt4 );
                (PInt4) += iInt4Skip;
            } else if ( iColumn == iInt5Column ) {
                ConcatInteger( sLine, (int *)PInt5 );
                (PInt5) += iInt5Skip;
            } else if ( iColumn == iInt6Column ) {
                ConcatInteger( sLine, (int *)PInt6 );
                (PInt6) += iInt6Skip;
            } else if ( iColumn == iInt7Column ) {
                ConcatInteger( sLine, (int *)PInt7 );
                (PInt7) += iInt7Skip;
            } else if ( iColumn == iInt8Column ) {
                ConcatInteger( sLine, (int *)PInt8 );
                (PInt8) += iInt8Skip;

            } else if ( iColumn == iDouble1Column ) {
                ConcatDouble( sLine, (double *)PDouble1 );
                (PDouble1) += iDouble1Skip;
            } else if ( iColumn == iDouble2Column ) {
                ConcatDouble( sLine, (double *)PDouble2 );
                (PDouble2) += iDouble2Skip;
            } else if ( iColumn == iDouble3Column ) {
                ConcatDouble( sLine, (double *)PDouble3 );
                (PDouble3) += iDouble3Skip;
            } else if ( iColumn == iDouble4Column ) {
                ConcatDouble( sLine, (double *)PDouble4 );
                (PDouble4) += iDouble4Skip;

            } else if ( iColumn == iString1Column ) {
                ConcatString( sLine, PString1 );
                (PString1) += iString1Skip;
            } else if ( iColumn == iString2Column ) {
                ConcatString( sLine, PString2 );
                (PString2) += iString2Skip;
            } else if ( iColumn == iString3Column ) {
                ConcatString( sLine, PString3 );
                (PString3) += iString3Skip;
            } else if ( iColumn == iString4Column ) {
                ConcatString( sLine, PString4 );
                (PString4) += iString4Skip;
            } else if ( iColumn == iString5Column ) {
                ConcatString( sLine, PString5 );
                (PString5) += iString5Skip;
            }
        }

                /* Write out the data */

        WriteDataLine( db, sLine );
    }
    

}


#endif          /* ifndef LINT */









/*
 *      DBClose
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Close the database file, compacting it if necessary
 */

void    DBClose( Pdb )
DATABASE*       Pdb;
{
DICTLOOP	dlEntries;
ENTRY		eCur;
char		*cPKey;


	/* If the file is a DB_SEQUENTIAL_ACCESS file */
	/* then just close it. */

    if ( (*Pdb)->iAccessMode == DB_SEQUENTIAL_ACCESS ) {
	fclose((*Pdb)->fDataBase);
    } else {

        /* Compact the file if something has been duplicated */

        if ( (*Pdb)->bCompactFileAtClose ) CompactDataBase((*Pdb));
        fclose((*Pdb)->fDataBase);

		/* Release memory for entries */

        dlEntries = dlDictLoop( (DICT)((*Pdb)->dEntries) );
        while ( (eCur = (ENTRY)vPDictNext( 
		 (DICT)((*Pdb)->dEntries), &dlEntries, &cPKey)) ) {
	    MESSAGE(( "Freeing entry: %s\n", eCur->sName ));
	    free(eCur);
        }
        DictDestroy( (DICT *)(&((*Pdb)->dEntries)) );
    }

		/* Free the DATABASE */

    free( (*Pdb) );
    *Pdb = NULL;

}





/*
 *      DBPushPrefix
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Concatenate the string to the current prefix and
 *      push the result onto the prefix stack.  All following
 *      entries will have the latest prefix attached to them.
 */

void    DBPushPrefix( db, sStr )
DATABASE        db;
char*           sStr;
{
String          sPrefix;



    sDataBaseName( db, sStr, sPrefix );
    db->iPrefix++;
    if ( db->iPrefix >= MAXPREFIXSTACK ) DFATAL(("Too many prefixes on stack"));
    strcpy( db->saPrefixStack[db->iPrefix], sPrefix );

}





/*
 *      DBPopPrefix
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Pop the current prefix from the prefix stack.
 */

void    DBPopPrefix( db )
DATABASE        db;
{


    db->iPrefix--;
    if ( db->iPrefix < 0 ) DFATAL(("Too many POPs from prefix stack"));

}




/*
 *      DBZeroPrefix
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *      Remove all prefixes from the prefix stack.
 */

void    DBZeroPrefix( db )
DATABASE        db;
{


    db->iPrefix = 0;
    strcpy( db->saPrefixStack[0], "" );

}



/*
 *      DBPushZeroPrefix
 *
 *	Author:	Christian Schafmeister (1991)
 *
 *	Push the prefix in (sStr) without concatenating
 *	to the current prefix.
 */

void    DBPushZeroPrefix( db, sStr )
DATABASE        db;
char*           sStr;
{



    db->iPrefix++;
    if ( db->iPrefix >= MAXPREFIXSTACK ) DFATAL(("Too many prefixes on stack"));
    strcpy( db->saPrefixStack[db->iPrefix], sStr );

}



