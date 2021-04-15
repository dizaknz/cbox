/*
 ============================================================================
 Name        : nzcsm_read.c
 Author      : ITHD
 Version     : 1
 Copyright   : NIWA
 Description : NZ Convective Scale Model (NZCSM) output reader
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <omp.h>
#include <netcdf.h>
#include <libpq-fe.h>

#include "nzcsm_read.h"
#include "logger.h"

void usage (void) {
    const char *usage = "NAME                                      \n\
       nzcsm_read                                                  \n\
                                                                   \n\
SYNOPSIS                                                           \n\
       nzcsm_read -i <NC file>                                     \n\
                                                                   \n\
DESCRIPTION                                                        \n\
       -i    NetCDF file to read                                   \n\
\n";

    fprintf (stdout, "%s", usage);
}

void handleError (int error) {
    log_error ("%s", nc_strerror (error));

    return;
}

/**
 * Loads to model_run
 */
int loadModelRunData () {
    return SUCCESS;
}

/**
 * Loads to forecast
 */
int loadForecastData () {
    return SUCCESS;
}

/**
 * Loads to data_series
 *
 * TODO: should we maintain forecast_variable?
 */
int loadVariableData () {
    return SUCCESS;
}

int processFile (const char *ncfile) {
    int status = NC_NOERR;
    int fh = 0;
    size_t hint = NC_SIZEHINT_DEFAULT;
    int ndim = 0, nvar = 0, natt = 0, udim = 0;

    /* shared */
    int dim = 0, var = 0, att = 0;
    bool cont = TRUE;

    /* private */
    char dname[NC_MAX_NAME] = { '\0' };
    size_t dsize = 0;

    log_info ("Opening: %s", ncfile);
    status = nc__open (ncfile, NC_NOWRITE, &hint, &fh);

    if (status != NC_NOERR) {
        handleError (status);
        return ERROR;
    }

    status = nc_inq(fh, &ndim, &nvar, &natt, &udim);

    if (status != NC_NOERR) {
        handleError (status);
        nc_close (fh);
        return ERROR;
    }

    log_info ("file=%s ndim=%d nvar=%d natt=%d udim=%d", ncfile, ndim, nvar, natt, udim);


    /* work sharing */
#pragma omp parallel for num_threads (ndim) private (dname, dsize)
    for (dim = 0; dim < ndim; dim++) {
        int tid = omp_get_thread_num();

        if (cont == TRUE) {
            status = nc_inq_dim (fh, dim, dname, &dsize);

            if (status != NC_NOERR) {
                handleError (status);
                cont = FALSE;
            }

            log_info ("thread=%d dim=%d name=%s size=%lu", tid, dim, dname, dsize);
        }
    }

    for (var = 0; var < nvar; var++) {
        if (processVariable (fh, var)) {
            break;
        }
    }

    log_info ("Closing: %s\n", ncfile);
    nc_close (fh);

    return 0;
}

int getDataTypeSize (nc_type typ) {
    switch (typ) {
        case NC_BYTE:
        case NC_CHAR:
        case NC_UBYTE:
            return 1;
        case NC_SHORT:
        case NC_USHORT:
            return 2;
        case NC_INT:
        case NC_UINT:
        case NC_FLOAT:
            return 4;
        case NC_INT64:
        case NC_UINT64:
        case NC_DOUBLE:
            return 8;
        default:
            return 0;
    }
}

int processVariable (int fh, int var) {
    int status = 0;
    char name[NC_MAX_NAME] = { '\0' };
    char dname[NC_MAX_NAME] = { '\0' };
    char aname[NC_MAX_NAME] = { '\0' };
    int ndims = 0;
    size_t dlen = 0;
    int dims[NC_MAX_VAR_DIMS];
    size_t dlens[NC_MAX_VAR_DIMS];
    int natt = 0;
    nc_type vtype = 0;
    int dim = 0;
    int att = 0;
    void *data;
    int dsize = 0;
    int tlen = 0;
    nc_type atype = 0;
    size_t alen = 0;
    int x = 0, y = 0, z = 0, t = 0;
    int nx = 0, ny = 0, nz = 0, nt = 0;
    int r = SUCCESS;
    int idx = 0;
    int offset = 0;

    status = nc_inq_var (fh, var, name, &vtype, &ndims, dims, &natt);

    if (status != NC_NOERR) {
        handleError (status);
        return ERROR;
    }

    log_info ("var=%d name=%s ndims=%d natt=%d vtype=%d", var, name, ndims, natt, vtype);

    if (vtype == NC_STRING) {
        /* do not support string types */
        log_error ("NZCSM do not support string variable: %s", name);
        return ERROR;
    }

    /* lookup size in bytes of a single value */
    dsize = getDataTypeSize (vtype);

    if (!dsize) {
        log_error ("Invalid data type size for variable: %s", name);
        return ERROR;
    }

    if (ndims) tlen = 1;

    for (dim = 0; dim < ndims; dim++) {
        /* lookup dimension */
        status = nc_inq_dim (fh, dims[dim], dname, &dlen);

        log_info ("var=%s dim=%s dlen=%lu [%d]", name, dname, dlen, dims[dim]);

        tlen *= dlen;
        dlens[dim] = dlen;
    }

    for (att = 0; att < natt; att++) {
        /* lookup attributes */
        nc_inq_attname (fh, var, att, aname);

        nc_inq_att (fh, var, aname, &atype, &alen);

        log_info ("var=%s att=%s type=%d len=%lu [%d]", name, aname, atype, alen, att);
    }

    log_info ("Allocating data with %d values of size %d ", tlen, dsize);

    if (!tlen) {
        /* no data to read */
        log_info ("No data to read for variable: %s", name);
        return SUCCESS;
    }

    data = calloc (tlen, dsize);

    if (!data) {
        log_error ("Out of memory");
        return ERROR;
    }

    /* read all of the data as contiguous block */
    if (vtype == NC_DOUBLE) {
        log_info ("nc_get_var_double");
        status = nc_get_var_double (fh, var, (double *)data);
    }

    if (status != NC_NOERR) {
        handleError (status);
        free (data);
        return ERROR;
    }

    switch (ndims) {
        case 0:
        case 1:
        case 2:
            /* skip 0, 1 and 2-D */
            nt = nz = ny = nx = 0;
            break;
        case 3:
            nt = dlens[0];
            nz = 0;
            ny = dlens[1];
            nx = dlens[2];
            break;
        case 4:
            nt = dlens[0];
            nz = dlens[1];
            ny = dlens[2];
            nx = dlens[3];
            break;
        default:
            log_error ("Invalid number of dimensions found: %d", ndims);
            return ERROR;
    }

    /* let's iterate through it */
    for (t = 0; t < nt; t++) {
        for (z = 0; z < nz; z++) {
            for (y = 0; y < ny; y++) {
                for (x = 0; x < nx; x++) {
                    if (vtype == NC_DOUBLE) {
                        if (nz) {
                            offset = (t * nz * ny * nx + z * ny * nz + y * nx + x) * dsize;
                        }
                        else {
                            offset = (t * ny * nx + y * nx + x) * dsize;
                        }
                        idx = offset/dsize;

                        if (idx > tlen) {
                            log_error ("Buffer overflow - idx: %d > tlen: %d", idx, tlen);
                        }
                        else {
                            double tmp = *((double *)(data + offset));

                            if (0)
                                fprintf (stdout, "[%d] %g ", idx, tmp);
                            ;
                        }

                        if (0) r = ERROR;
                    }
                }
                if (0)
                    if (vtype == NC_DOUBLE) {
                        fprintf (stdout, "\n");
                    }
            }
        }
    }

    if (data) {
        free (data);
    }

    return r;
}

/**
 *
 */
int main (int argc, char *argv[]) {
    char c = 0;
    char *ncfile = NULL;

    /* parse command line */
    while ((c = getopt (argc, argv, "i:h")) != -1) {
        switch (c) {
            case 'i':
                ncfile = strdup ((char *)optarg);
                break;
            case 'h':
                usage();
                return SUCCESS;
            case '?':
                fprintf (stderr, "Invalid option: %c\n", c);
                return ERROR;
            default:
                usage();
                return SUCCESS;
        }
    }

    if (argc == 1) {
        usage();
        return SUCCESS;
    }

    processFile (ncfile);

    if (ncfile) {
        free (ncfile);
    }

    return SUCCESS;
}


