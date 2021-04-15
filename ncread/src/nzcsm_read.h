/*
 * nzcsm_read.h
 *
 *  Created on: Jan 20, 2015
 *      Author: deyzelit
 */

#ifndef NZCSM_READ_H_
#define NZCSM_READ_H_

typedef unsigned char bool;

#define ERROR 1
#define SUCCESS 0
#define TRUE 1
#define FALSE 0

/** Handle Error
 *
 *  @param[in] error NetCDF error code
 */
void handleError (int error);

/** Process NetCDF file
 *
 *  @param[in] ncfile name of netCDF file to read and process
 */
int processFile (const char *ncfile);

/** Process a NetCDF variable
 *
 *  @param[in] fh NetCDF file handle
 *  @param[in] var NetCDF variable index
 */
int processVariable (int fh, int var);

/** Lookup NC data type size in bytes
 *
 *  @param[in] typ NetCDF nc_type
 */
int getDataTypeSize (nc_type typ);

#endif /* NZCSM_READ_H_ */
