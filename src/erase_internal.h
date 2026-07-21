#ifndef DFX_ERASE_INTERNAL_H
#define DFX_ERASE_INTERNAL_H

#include "dfx.h"

#include <stdio.h>

int dfx_validate_open_target(FILE *file, const dfx_job *job, char error[DFX_ERROR_MAX]);

#endif
