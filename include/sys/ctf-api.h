/* Public API to libctf.
   Copyright (c) 2005, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2.  */

/* This header file defines the interfaces available from the CTF debugger
   library, libctf.  This API can be used by a debugger to operate on data in
   the Compact ANSI-C Type Format (CTF).  */

#ifndef	_CTF_API_H
#define	_CTF_API_H

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ctf.h>
#include <zlib.h>

#ifdef	__cplusplus
extern "C"
  {
#endif

/* Clients can open one or more CTF containers and obtain a pointer to an
   opaque ctf_file_t.  Types are identified by an opaque ctf_id_t token.
   They can also open or create read-only archives of CTF containers in a
   ctf_archive_t.

   These opaque definitions allow libctf to evolve without breaking clients.  */

typedef struct ctf_file ctf_file_t;
typedef struct ctf_archive ctf_archive_t;
typedef long ctf_id_t;

/* If the debugger needs to provide the CTF library with a set of raw buffers
   for use as the CTF data, symbol table, and string table, it can do so by
   filling in ctf_sect_t structures and passing them to ctf_bufopen().  */

typedef struct ctf_sect
{
  const char *cts_name;		  /* Section name (if any).  */
  unsigned long cts_type;	  /* Section type (ELF SHT_... value).  */
  unsigned long cts_flags;	  /* Section flags (ELF SHF_... value).  */
  const void *cts_data;		  /* Pointer to section data.  */
  size_t cts_size;		  /* Size of data in bytes.  */
  size_t cts_entsize;		  /* Size of each section entry (symtab only).  */
  off64_t cts_offset;		  /* File offset of this section (if any).  */
} ctf_sect_t;

/* Encoding information for integers, floating-point values, and certain other
   intrinsics can be obtained by calling ctf_type_encoding(), below.  The flags
   field will contain values appropriate for the type defined in <sys/ctf.h>.  */

typedef struct ctf_encoding
{
  uint32_t cte_format;		 /* Data format (CTF_INT_* or CTF_FP_* flags).  */
  uint32_t cte_offset;		 /* Oofset of value in bits.  */
  uint32_t cte_bits;		 /* Size of storage in bits.  */
} ctf_encoding_t;

typedef struct ctf_membinfo
{
  ctf_id_t ctm_type;		/* Type of struct or union member.  */
  unsigned long ctm_offset;	/* Offset of member in bits.  */
} ctf_membinfo_t;

typedef struct ctf_arinfo
{
  ctf_id_t ctr_contents;	/* Type of array contents.  */
  ctf_id_t ctr_index;		/* Type of array index.  */
  uint32_t ctr_nelems;		/* Number of elements.  */
} ctf_arinfo_t;

typedef struct ctf_funcinfo
{
  ctf_id_t ctc_return;		/* Function return type.  */
  uint32_t ctc_argc;		/* Number of typed arguments to function.  */
  uint32_t ctc_flags;		/* Function attributes (see below).  */
} ctf_funcinfo_t;

typedef struct ctf_lblinfo
{
  ctf_id_t ctb_typeidx;		/* Last type associated with the label.  */
} ctf_lblinfo_t;

typedef struct ctf_snapshot_id
{
  unsigned long dtd_id;		/* Highest DTD ID at time of snapshot.  */
  unsigned long snapshot_id;	/* Snapshot id at time of snapshot.  */
} ctf_snapshot_id_t;

#define	CTF_FUNC_VARARG	0x1	/* Function arguments end with varargs.  */

/* Functions that return integer status or a ctf_id_t use the following value
   to indicate failure.  ctf_errno() can be used to obtain an error code.  */
#define	CTF_ERR	(-1L)

#define	ECTF_BASE	1000	/* Base value for libctf errnos.  */

enum
  {
   ECTF_FMT = ECTF_BASE,	/* File is not in CTF or ELF format.  */
   ECTF_ELFVERS,		/* ELF version is more recent than libctf.  */
   ECTF_CTFVERS,		/* CTF version is more recent than libctf.  */
   ECTF_ENDIAN,			/* Data is different endian-ness than lib.  */
   ECTF_SYMTAB,			/* Symbol table uses invalid entry size.  */
   ECTF_SYMBAD,			/* Symbol table data buffer invalid.  */
   ECTF_STRBAD,			/* String table data buffer invalid.  */
   ECTF_CORRUPT,		/* File data corruption detected.  */
   ECTF_NOCTFDATA,		/* ELF file does not contain CTF data.  */
   ECTF_NOCTFBUF,		/* Buffer does not contain CTF data.  */
   ECTF_NOSYMTAB,		/* Symbol table data is not available.  */
   ECTF_NOPARENT,		/* Parent CTF container is not available.  */
   ECTF_DMODEL,			/* Data model mismatch.  */
   ECTF_MMAP,			/* Failed to mmap a data section.  */
   ECTF_ZALLOC,			/* Failed to allocate (de)compression buffer.  */
   ECTF_DECOMPRESS,		/* Failed to decompress CTF data.  */
   ECTF_STRTAB,			/* String table for this string is missing.  */
   ECTF_BADNAME,		/* String offset is corrupt w.r.t. strtab.  */
   ECTF_BADID,			/* Invalid type ID number.  */
   ECTF_NOTSOU,			/* Type is not a struct or union.  */
   ECTF_NOTENUM,		/* Type is not an enum.  */
   ECTF_NOTSUE,			/* Type is not a struct, union, or enum.  */
   ECTF_NOTINTFP,		/* Type is not an integer or float.  */
   ECTF_NOTARRAY,		/* Type is not an array.  */
   ECTF_NOTREF,			/* Type does not reference another type.  */
   ECTF_NAMELEN,		/* Buffer is too small to hold type name.  */
   ECTF_NOTYPE,			/* No type found corresponding to name.  */
   ECTF_SYNTAX,			/* Syntax error in type name.  */
   ECTF_NOTFUNC,		/* Symtab entry does not refer to a function.  */
   ECTF_NOFUNCDAT,		/* No func info available for function.  */
   ECTF_NOTDATA,		/* Symtab entry does not refer to a data obj.  */
   ECTF_NOTYPEDAT,		/* No type info available for object.  */
   ECTF_NOLABEL,		/* No label found corresponding to name.  */
   ECTF_NOLABELDATA,		/* File does not contain any labels.  */
   ECTF_NOTSUP,			/* Feature not supported.  */
   ECTF_NOENUMNAM,		/* Enum element name not found.  */
   ECTF_NOMEMBNAM,		/* Member name not found.  */
   ECTF_RDONLY,			/* CTF container is read-only.  */
   ECTF_DTFULL,			/* CTF type is full (no more members allowed).  */
   ECTF_FULL,			/* CTF container is full.  */
   ECTF_DUPLICATE,		/* Duplicate member or variable name.  */
   ECTF_CONFLICT,		/* Conflicting type definition present.  */
   ECTF_OVERROLLBACK,		/* Attempt to roll back past a ctf_update.  */
   ECTF_COMPRESS,		/* Failed to compress CTF data.  */
   ECTF_ARCREATE,		/* Error creating CTF archive.  */
   ECTF_ARNNAME			/* Name not found in CTF archive.  */
  };

/* The CTF data model is inferred to be the caller's data model or the data
   model of the given object, unless ctf_setmodel() is explicitly called.  */
#define	CTF_MODEL_ILP32 1	/* Object data model is ILP32.  */
#define	CTF_MODEL_LP64  2	/* Object data model is LP64.  */
#ifdef _LP64
# define CTF_MODEL_NATIVE CTF_MODEL_LP64
#else
# define CTF_MODEL_NATIVE CTF_MODEL_ILP32
#endif

/* Dynamic CTF containers can be created using ctf_create().  The ctf_add_*
   routines can be used to add new definitions to the dynamic container.
   New types are labeled as root or non-root to determine whether they are
   visible at the top-level program scope when subsequently doing a lookup.  */

#define	CTF_ADD_NONROOT	0	/* Type only visible in nested scope.  */
#define	CTF_ADD_ROOT	1	/* Type visible at top-level scope.  */

/* These typedefs are used to define the signature for callback functions
   that can be used with the iteration and visit functions below.  */

typedef int ctf_visit_f (const char *, ctf_id_t, unsigned long, int, void *);
typedef int ctf_member_f (const char *, ctf_id_t, unsigned long, void *);
typedef int ctf_enum_f (const char *, int, void *);
typedef int ctf_variable_f (const char *, ctf_id_t, void *);
typedef int ctf_type_f (ctf_id_t, void *);
typedef int ctf_label_f (const char *, const ctf_lblinfo_t *, void *);
typedef int ctf_archive_member_f (ctf_file_t *, const char *name, void *);
typedef int ctf_archive_raw_member_f (const char *name, const void *content,
				      size_t len, void *);

extern ctf_file_t *ctf_bufopen (const ctf_sect_t *, const ctf_sect_t *,
				const ctf_sect_t *, int *);
extern ctf_file_t *ctf_fdopen (int, int *);
extern ctf_file_t *ctf_open (const char *, int *);
extern ctf_file_t *ctf_create (int *);
extern void ctf_close (ctf_file_t *);
extern ctf_sect_t ctf_getdatasect (const ctf_file_t *);

extern int ctf_arc_write (const char *, ctf_file_t **, size_t,
			  const char **, size_t);
extern ctf_archive_t *ctf_arc_open (const char *, int *);
extern void ctf_arc_close (ctf_archive_t *);
extern ctf_file_t *ctf_arc_open_by_name (const ctf_archive_t *,
					 const char *, int *);

extern ctf_file_t *ctf_parent_file (ctf_file_t *);
extern const char *ctf_parent_name (ctf_file_t *);
extern void ctf_parent_name_set (ctf_file_t *, const char *);
extern int ctf_type_isparent (ctf_file_t *, ctf_id_t);
extern int ctf_type_ischild (ctf_file_t *, ctf_id_t);

extern int ctf_import (ctf_file_t *, ctf_file_t *);
extern int ctf_setmodel (ctf_file_t *, int);
extern int ctf_getmodel (ctf_file_t *);

extern void ctf_setspecific (ctf_file_t *, void *);
extern void *ctf_getspecific (ctf_file_t *);

extern int ctf_errno (ctf_file_t *);
extern const char *ctf_errmsg (int);
extern int ctf_version (int);

extern int ctf_func_info (ctf_file_t *, unsigned long, ctf_funcinfo_t *);
extern int ctf_func_args (ctf_file_t *, unsigned long, uint32_t, ctf_id_t *);

extern ctf_id_t ctf_lookup_by_name (ctf_file_t *, const char *);
extern ctf_id_t ctf_lookup_by_symbol (ctf_file_t *, unsigned long);
extern ctf_id_t ctf_lookup_variable (ctf_file_t *, const char *);

extern ctf_id_t ctf_type_resolve (ctf_file_t *, ctf_id_t);
extern ssize_t ctf_type_lname (ctf_file_t *, ctf_id_t, char *, size_t);
extern char *ctf_type_name (ctf_file_t *, ctf_id_t, char *, size_t);
extern ssize_t ctf_type_size (ctf_file_t *, ctf_id_t);
extern ssize_t ctf_type_align (ctf_file_t *, ctf_id_t);
extern int ctf_type_kind (ctf_file_t *, ctf_id_t);
extern ctf_id_t ctf_type_reference (ctf_file_t *, ctf_id_t);
extern ctf_id_t ctf_type_pointer (ctf_file_t *, ctf_id_t);
extern int ctf_type_encoding (ctf_file_t *, ctf_id_t, ctf_encoding_t *);
extern int ctf_type_visit (ctf_file_t *, ctf_id_t, ctf_visit_f *, void *);
extern int ctf_type_cmp (ctf_file_t *, ctf_id_t, ctf_file_t *, ctf_id_t);
extern int ctf_type_compat (ctf_file_t *, ctf_id_t, ctf_file_t *, ctf_id_t);

extern int ctf_member_info (ctf_file_t *, ctf_id_t, const char *,
			    ctf_membinfo_t *);
extern int ctf_array_info (ctf_file_t *, ctf_id_t, ctf_arinfo_t *);

extern const char *ctf_enum_name (ctf_file_t *, ctf_id_t, int);
extern int ctf_enum_value (ctf_file_t *, ctf_id_t, const char *, int *);

extern void ctf_label_set (ctf_file_t *, const char *);
extern const char *ctf_label_get (ctf_file_t *);

extern const char *ctf_label_topmost (ctf_file_t *);
extern int ctf_label_info (ctf_file_t *, const char *, ctf_lblinfo_t *);

extern int ctf_member_iter (ctf_file_t *, ctf_id_t, ctf_member_f *, void *);
extern int ctf_enum_iter (ctf_file_t *, ctf_id_t, ctf_enum_f *, void *);
extern int ctf_type_iter (ctf_file_t *, ctf_type_f *, void *);
extern int ctf_label_iter (ctf_file_t *, ctf_label_f *, void *);
extern int ctf_variable_iter (ctf_file_t *, ctf_variable_f *, void *);
extern int ctf_archive_iter (const ctf_archive_t *, ctf_archive_member_f *,
			     void *);
extern int ctf_archive_raw_iter (const ctf_archive_t *,
				 ctf_archive_raw_member_f *, void *);

extern ctf_id_t ctf_add_array (ctf_file_t *, uint32_t,
			       const ctf_arinfo_t *);
extern ctf_id_t ctf_add_const (ctf_file_t *, uint32_t, ctf_id_t);
extern ctf_id_t ctf_add_enum (ctf_file_t *, uint32_t, const char *);
extern ctf_id_t ctf_add_float (ctf_file_t *, uint32_t,
			       const char *, const ctf_encoding_t *);
extern ctf_id_t ctf_add_forward (ctf_file_t *, uint32_t, const char *,
				 uint32_t);
extern ctf_id_t ctf_add_function (ctf_file_t *, uint32_t,
				  const ctf_funcinfo_t *, const ctf_id_t *);
extern ctf_id_t ctf_add_integer (ctf_file_t *, uint32_t, const char *,
				 const ctf_encoding_t *);
extern ctf_id_t ctf_add_pointer (ctf_file_t *, uint32_t, ctf_id_t);
extern ctf_id_t ctf_add_type (ctf_file_t *, ctf_file_t *, ctf_id_t);
extern ctf_id_t ctf_add_typedef (ctf_file_t *, uint32_t, const char *,
				 ctf_id_t);
extern ctf_id_t ctf_add_restrict (ctf_file_t *, uint32_t, ctf_id_t);
extern ctf_id_t ctf_add_struct (ctf_file_t *, uint32_t, const char *);
extern ctf_id_t ctf_add_union (ctf_file_t *, uint32_t, const char *);
extern ctf_id_t ctf_add_struct_sized (ctf_file_t *, uint32_t, const char *,
				      size_t);
extern ctf_id_t ctf_add_union_sized (ctf_file_t *, uint32_t, const char *,
				     size_t);
extern ctf_id_t ctf_add_volatile (ctf_file_t *, uint32_t, ctf_id_t);

extern int ctf_add_enumerator (ctf_file_t *, ctf_id_t, const char *, int);
extern int ctf_add_member (ctf_file_t *, ctf_id_t, const char *, ctf_id_t);
extern int ctf_add_member_offset (ctf_file_t *, ctf_id_t, const char *,
				  ctf_id_t, unsigned long);

extern int ctf_add_variable (ctf_file_t *, const char *, ctf_id_t);

extern int ctf_set_array (ctf_file_t *, ctf_id_t, const ctf_arinfo_t *);

extern int ctf_update (ctf_file_t *);
extern ctf_snapshot_id_t ctf_snapshot (ctf_file_t *);
extern int ctf_rollback (ctf_file_t *, ctf_snapshot_id_t);
extern int ctf_discard (ctf_file_t *);
extern int ctf_write (ctf_file_t *, int);
extern int ctf_gzwrite (ctf_file_t * fp, gzFile fd);
extern int ctf_compress_write (ctf_file_t * fp, int fd);

#ifdef	__cplusplus
}
#endif

#endif				/* _CTF_API_H */
