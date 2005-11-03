/*********************************************************************************/
/* Copyright 2002 Christian Casteyde (casteyde.christian@free.fr)                */
/*                                                                               */
/* "buildDSP" is free software; you can redistribute it                          */
/* and/or modify it under the terms of the GNU General Public License as         */
/* published by the Free Software Foundation; either version 2 of the License,   */
/* or (at your option) any later version.                                        */
/*                                                                               */
/* "ADI USB ADSL Driver for Linux" is distributed in the hope that it will be    */
/* useful, but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 */
/* GNU General Public License for more details.                                  */
/*                                                                               */
/* You should have received a copy of the GNU General Public License             */
/* along with "ADI USB ADSL Driver for Linux"; if not, write to the Free Software*/
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     */
/*********************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>
#include <dirent.h>

#define MAXFILENAMELEN	16

/*
    Help print function.
 */
static void Usage()
{
    printf( "Usage : buildDSP [-h | -v | [-d dir] file]\n\n"
	    "Write DSP code of BNM files in binary file \"file\".\n\n"
	    "Options:\n"
	    "-h          Print this help screen\n"
	    "-v          Print version information\n"
	    "-d dir      Specify input directory\n");
}

/*
    Version print function.
 */
static void Version()
{
    printf( "buildDSP version 1.1.0\n\n"
	    "Copyright 2002 Christian Casteyde\n"
	    "This program is free software; you can redistribute it and/or modify it\n"
	    "under the terms of the GNU General Public License as published\n"
	    "by the Free Software Foundation; either version 2 of the License,\n"
	    "or (at your option) any later version.\n");
}

/*
    Function to convert an ASCII hexadecimal character
into the corresponding byte.
    Input:
	The ASCII hexadecimal character.
    Output:
	Its binary value.
 */
static uint8_t HexToByte(char c)
{
    int i;
    switch (c)
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
	i = (c - '0');
	break;
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
	i = (c - 'A') + 10;
	break;
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
	i = (c - 'a') + 10;
	break;
    default:
	assert(0);
	break;
    }
    return i;
}

/*
    Function to read two bytes from the ASCII string and convert it in a byte.
    Input:
	The ASCII hexadecimal string
    Output:
	The binary value of the first byte.
 */
static uint8_t ReadByte(const char *str)
{
    return (HexToByte(str[0]) << 4) + HexToByte(str[1]);
}

/*
    Function to add a S-record in the DSP code buffer.
    Input:
	Pointer to the record buffer.
	Maximum size of the record buffer.
	Pointer to base pointer of the DSP code buffer (can be reallocated).
	Pointer to the currently known DSP buffer size (can be updated
if a reallocation occurs).
    Output:
	The number of character consumed in the record buffer.
 */

static long PutSRecordInDSPBuffer(const char *pRecord, size_t sMaxSize,
    uint8_t **ppBuffer, size_t *pBufferSize)
{
    long lConsumed = 0;
    /* S record headers are 4 bytes long: */
    assert(pRecord != NULL);
    assert(sMaxSize >= 4);
    assert(pBufferSize != NULL);
    assert(ppBuffer != NULL);
    if ('S' == pRecord[0]  || 's' == pRecord[0])
    {
	/* Get the record length: */
	uint8_t bByte = ReadByte(pRecord + 2);
	uint8_t bChecksum = bByte;
	int iDataLength = bByte;
	if (sMaxSize >= iDataLength * 2)
	{
	    int i;
	    /* Skip the header: */
	    lConsumed = lConsumed + 4;
	    /* Parse the S-record: */
	    switch (pRecord[1])
	    {
	    case '2':
	    case '3':
		{
		    size_t iOffset = 0;
		    size_t iOffsetLen;
		    if ('2' == pRecord[1])
			iOffsetLen = 3;
		    else
			iOffsetLen = 4;
		    /* Get the offset and remove offset data: */
		    if (iDataLength >= iOffsetLen + 1)	/* Offset length + checksum */
		    {
			for (i=0; i<iOffsetLen; ++i)
			{
			    iOffset = iOffset<<8;
			    bByte = ReadByte(pRecord+4+2*i);
			    bChecksum += bByte;
			    iOffset += bByte;
			}
			iDataLength = iDataLength - iOffsetLen;
			lConsumed = lConsumed + iOffsetLen*2;
		    }
		    else
			iOffset = -1;
		    if (iOffset != -1)
		    {
			/* Get more memory for DSP code if necessary: */
			if (*pBufferSize < iOffset + iDataLength - 1)	/* Checksum must not be read. */
			{
			    uint8_t *pNewBuffer = (uint8_t *) realloc(*ppBuffer, iOffset + iDataLength - 1);
			    if (pNewBuffer != NULL)
			    {
				*ppBuffer = pNewBuffer;
				*pBufferSize = iOffset + iDataLength - 1;
			    }
			    else
			    {
				fprintf(stderr, "Out of memory while reallocating DSP buffer!\n");
			    }
			}
			/* If DSP code buffer is large enough, fill it: */
			if (*pBufferSize >= iOffset + iDataLength - 1)	/* Checksum must not be read. */
			{
			    for (i=0; i<iDataLength-1; ++i)
			    {
				bByte = ReadByte(pRecord + lConsumed);
				bChecksum += bByte;
				(*ppBuffer)[iOffset + i] = bByte;
				lConsumed = lConsumed + 2;
			    }
			}
		    }
		    else
		    {
			fprintf(stderr, "Not enough data in S-record to read offset field!\n");
		    }
		}
		break;
	    case '9':	/* End of recordset. */
		{
		    /* Compute the checksum: */
		    for (i=0; i<iDataLength-1; ++i)
		    {
			bByte = ReadByte(pRecord + lConsumed);
			bChecksum += bByte;
			lConsumed += 2;
		    }
		}
		break;
	    default:
		fprintf(stderr, "Unknown record type '%c'!\n", pRecord[1]);
		break;
	    }
	    /* Read the checksum: */
	    bByte = ReadByte(pRecord + lConsumed);
	    lConsumed = lConsumed + 2;
	    /* Skip the CR/LF end of line marker: */
	    lConsumed = lConsumed + 2;
	    /* Verify the checksum: */
	    bChecksum = ~bChecksum;
	    if (bByte != bChecksum)
	    {
		fprintf(stderr, "Invalid checksum un S-record (read 0x%02X, should be 0x%02X)!\n",
		    bByte, bChecksum);
	    }
	}
	else
	{
	    fprintf(stderr, "End of file encountered while parsing S-record!\n");
	}
    }
    else
    {
	fprintf(stderr, "Record buffer doesn't contain a valid S-record!\n");
    }
    return lConsumed;
}

/*
    Function to load a full S-record file in the DSP buffer.
    Input:
	Name of the file to load.
	Pointer to the base pointer to the destination buffer.
	Pointer to the destination buffer size.
    Output:
	Offset specified in the first S-record of the file.
 */
static size_t LoadSRecordFile(const char *szFileName, uint8_t **ppBuffer, size_t *pBufferSize)
{
    size_t iFirstOffset = 0;
    FILE *f = fopen(szFileName, "rb");
    if (NULL != f)
    {
	long lSize = 0;
	char *pFileData = NULL;
	/* Get the file size: */
	fseek(f, 0, SEEK_END);
	lSize = ftell(f);
	fseek(f, 0, SEEK_SET);
	pFileData = (char *) malloc(lSize);
	if (NULL != pFileData)
	{
	    /* Read the file data: */
	    if (fread(pFileData, sizeof(char), lSize, f) == lSize)
	    {
		/* Check for file validity: */
		if (lSize >= 4 && ('S' == pFileData[0] || 's' == pFileData[0]))
		{
		    /* Try to get the first S-record offset: */
		    size_t iOffsetLen;
		    if ('2' == pFileData[1])
			iOffsetLen = 3;
		    else
			iOffsetLen = 4;
		    if (lSize >= 4+2*iOffsetLen+2)	/* Header + offset size + checksum size */
		    {
			int i;
			for (i=0; i<iOffsetLen; ++i)
			{
			    iFirstOffset = iFirstOffset<<8;
			    iFirstOffset += ReadByte(pFileData+4+2*i);
			}
		    }
		    else
			iFirstOffset = -1;
		    if (iFirstOffset != -1)
		    {
			/* Load DSP code: */
			long lCurrent = 0;
			while (lCurrent < lSize)
			    lCurrent += PutSRecordInDSPBuffer(pFileData+lCurrent, lSize-lCurrent,
				ppBuffer, pBufferSize);
		    }
		    else
		    {
			fprintf(stderr, "End of file encountered while parsing file \"%s\"!\n",
			    szFileName);
		    }
		}
		else
		{
		    fprintf(stderr, "\"%s\" is not a S-record file!\n", szFileName);
		}
	    }
	    else
	    {
		fprintf(stderr, "Unable to read file \"%s\"!\n", szFileName);
	    }
	    free(pFileData);
	}
	else
	{
	    fprintf(stderr, "Not enough memory to load file \"%s\"!\n", szFileName);
	}
    }
    else
    {
	fprintf(stderr, "Unable to open file \"%s\" for reading!\n", szFileName);
    }
    return iFirstOffset;
}

/*
    IsBNMFileEntry.
    Tells if a directory entry references a BNM file.
    BNM files are identified by their extension (.bnm).
    Input:
	Directory entry of the file.
    Output:
	1 if the file is a BNM file, 0 otherwise.
 */

int IsBNMFileEntry(const struct dirent *pEntry)
{
    int iResult = 0;
    const char *pExtension = rindex(pEntry->d_name, '.');
    if (pExtension != NULL)
    {
	if (strcmp(pExtension, ".bnm") == 0)
	    iResult = 1;
    }
    return iResult;
}

/*
    WriteDSPFile.
    Loads all BNM files from specified directory and writes the DSP code binary file.
    The DSP file format is the following:

    IDMA offset		4 bytes in network representation
    binary DSP code	variable
 */
void WriteDSPFile(const char *szDestFile, const char *szBNMPath, int iPathLength)
{
    char *szFileName = (char *) malloc((iPathLength + MAXFILENAMELEN + 2) * sizeof(char));
    if (NULL != szFileName)
    {
	struct dirent **aEntries = NULL;
	int iEntries;
	sprintf(szFileName, "%s/", szBNMPath);
	iEntries = scandir(szBNMPath, &aEntries, &IsBNMFileEntry, &alphasort);
	if (iEntries >= 0)
	{
	    uint32_t uiIDMAOffset = 0;
	    uint8_t *pBuffer = NULL;
	    size_t sBufferSize = 0;
	    FILE *f;
	    /* Read the BNM files: */
	    int i, iFirst = 1;
	    for (i=0; i<iEntries; ++i)
	    {
		uint32_t uiResult;
		strncpy(szFileName + iPathLength + 1, aEntries[i]->d_name, MAXFILENAMELEN + 1);
		szFileName[iPathLength + MAXFILENAMELEN + 1] = 0;
		fprintf(stdout, "Reading file %s\n", szFileName);
		uiResult = LoadSRecordFile(szFileName, &pBuffer, &sBufferSize);
		if (iFirst)
		{
		    uiIDMAOffset = htonl(uiResult);
		    iFirst = 0;
		}
		free(aEntries[i]);
	    }
	    free(aEntries);
	    /* Now writes the DSP code: */
	    f = fopen(szDestFile, "wb");
	    if (f != NULL)
	    {
		if (fwrite(&uiIDMAOffset, sizeof(uiIDMAOffset), 1, f) == 1)
		{
		    fprintf(stdout, "Writing file %s\n", szDestFile);
		    if (fwrite(pBuffer, 1, sBufferSize, f) != sBufferSize)
		    {
			fprintf(stderr, "Unable to write %lu bytes in file \"%s\"!\n",
			    (unsigned long)sBufferSize, szDestFile);
		    }
		}
		else
		{
		    fprintf(stderr, "Unable to write IDMA offset in file \"%s\"!\n", szDestFile);
		}
		fclose(f);
	    }
	    else
	    {
		fprintf(stderr, "Unable to open file \"%s\" for writing!\n", szDestFile);
	    }
	    free(pBuffer);
	}
	else
	    perror("Unable to read BNM files");
	free(szFileName);
    }
    else
	fprintf(stderr, "Out of memory.\n");
    return ;
}

/*
    Main function.

    Parse command line input and process files.
 */
int main(int iArgc, char *aArgv[])
{
    if (1 == iArgc)
    {
	Usage();
    }
    else
    if (2 == iArgc)
    {
	if (strcmp("-v", aArgv[1]) == 0)
	    Version();
	else
	if (strcmp("-h", aArgv[1]) == 0)
	    Usage();
	else
	    WriteDSPFile(aArgv[1], ".", 1);
    }
    else
    if (4 == iArgc)
    {
	if (strcmp("-d", aArgv[1]) == 0)
	    WriteDSPFile(aArgv[3], aArgv[2], strlen(aArgv[2]));
	else
	    Usage();
    }
    else
	Usage();
    return 0;
}
