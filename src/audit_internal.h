#ifndef DFX_AUDIT_INTERNAL_H
#define DFX_AUDIT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

bool dfx_audit_record_syntax_valid(const unsigned char *data, size_t size);

#endif
