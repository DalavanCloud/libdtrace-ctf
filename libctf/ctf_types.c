/* Type handling functions.
   Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#include <sys/compiler.h>
#include <ctf_impl.h>
#include <string.h>

/* Determine whether a type is a parent or a child.  */

int
ctf_type_isparent (ctf_file_t *fp, ctf_id_t id)
{
  return (LCTF_TYPE_ISPARENT (fp, id));
}

int
ctf_type_ischild (ctf_file_t * fp, ctf_id_t id)
{
  return (LCTF_TYPE_ISCHILD (fp, id));
}

/* Iterate over the members of a STRUCT or UNION.  We pass the name, member
   type, and offset of each member to the specified callback function.  */

int
ctf_member_iter (ctf_file_t *fp, ctf_id_t type, ctf_member_f *func, void *arg)
{
  ctf_file_t *ofp = fp;
  const ctf_type_t *tp;
  ssize_t size, increment;
  uint32_t kind, n;
  int rc;

  if ((type = ctf_type_resolve (fp, type)) == CTF_ERR)
    return CTF_ERR;		/* errno is set for us.  */

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  (void) ctf_get_ctt_size (fp, tp, &size, &increment);
  kind = LCTF_INFO_KIND (fp, tp->ctt_info);

  if (kind != CTF_K_STRUCT && kind != CTF_K_UNION)
    return (ctf_set_errno (ofp, ECTF_NOTSOU));

  if (size < CTF_LSTRUCT_THRESH)
    {
      const ctf_member_t *mp = (const ctf_member_t *) ((uintptr_t) tp +
						       increment);

      for (n = LCTF_INFO_VLEN (fp, tp->ctt_info); n != 0; n--, mp++)
	{
	  const char *name = ctf_strptr (fp, mp->ctm_name);
	  if ((rc = func (name, mp->ctm_type, mp->ctm_offset, arg)) != 0)
	    return rc;
	}

    }
  else
    {
      const ctf_lmember_t *lmp = (const ctf_lmember_t *) ((uintptr_t) tp +
							  increment);

      for (n = LCTF_INFO_VLEN (fp, tp->ctt_info); n != 0; n--, lmp++)
	{
	  const char *name = ctf_strptr (fp, lmp->ctlm_name);
	  if ((rc = func (name, lmp->ctlm_type,
			  (unsigned long) CTF_LMEM_OFFSET (lmp), arg)) != 0)
	    return rc;
	}
    }

  return 0;
}

/* Iterate over the members of an ENUM.  We pass the string name and associated
   integer value of each enum element to the specified callback function.  */

int
ctf_enum_iter (ctf_file_t *fp, ctf_id_t type, ctf_enum_f *func, void *arg)
{
  ctf_file_t *ofp = fp;
  const ctf_type_t *tp;
  const ctf_enum_t *ep;
  ssize_t increment;
  uint32_t n;
  int rc;

  if ((type = ctf_type_resolve (fp, type)) == CTF_ERR)
    return CTF_ERR;		/* errno is set for us.  */

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  if (LCTF_INFO_KIND (fp, tp->ctt_info) != CTF_K_ENUM)
    return (ctf_set_errno (ofp, ECTF_NOTENUM));

  (void) ctf_get_ctt_size (fp, tp, NULL, &increment);

  ep = (const ctf_enum_t *) ((uintptr_t) tp + increment);

  for (n = LCTF_INFO_VLEN (fp, tp->ctt_info); n != 0; n--, ep++)
    {
      const char *name = ctf_strptr (fp, ep->cte_name);
      if ((rc = func (name, ep->cte_value, arg)) != 0)
	return rc;
    }

  return 0;
}

/* Iterate over every root (user-visible) type in the given CTF container.
   We pass the type ID of each type to the specified callback function.  */

int
ctf_type_iter (ctf_file_t *fp, ctf_type_f *func, void *arg)
{
  ctf_id_t id, max = fp->ctf_typemax;
  int rc, child = (fp->ctf_flags & LCTF_CHILD);

  for (id = 1; id <= max; id++)
    {
      const ctf_type_t *tp = LCTF_INDEX_TO_TYPEPTR (fp, id);
      if (LCTF_INFO_ISROOT (fp, tp->ctt_info)
	  && (rc = func (LCTF_INDEX_TO_TYPE (fp, id, child), arg)) != 0)
	return rc;
    }

  return 0;
}

/* Iterate over every variable in the given CTF container, in arbitrary order.
   We pass the name of each variable to the specified callback function.  */

int
ctf_variable_iter (ctf_file_t *fp, ctf_variable_f *func, void *arg)
{
  unsigned long i;
  int rc;

  if ((fp->ctf_flags & LCTF_CHILD) && (fp->ctf_parent == NULL))
    return ECTF_NOPARENT;

  for (i = 0; i < fp->ctf_nvars; i++)
    if ((rc = func (ctf_strptr (fp, fp->ctf_vars[i].ctv_name),
		    fp->ctf_vars[i].ctv_typeidx, arg)) != 0)
      return rc;

  return 0;
}

/* Follow a given type through the graph for TYPEDEF, VOLATILE, CONST, and
   RESTRICT nodes until we reach a "base" type node.  This is useful when
   we want to follow a type ID to a node that has members or a size.  To guard
   against infinite loops, we implement simplified cycle detection and check
   each link against itself, the previous node, and the topmost node.  */

ctf_id_t
ctf_type_resolve (ctf_file_t * fp, ctf_id_t type)
{
  ctf_id_t prev = type, otype = type;
  ctf_file_t *ofp = fp;
  const ctf_type_t *tp;

  while ((tp = ctf_lookup_by_id (&fp, type)) != NULL)
    {
      switch (LCTF_INFO_KIND (fp, tp->ctt_info))
	{
	case CTF_K_TYPEDEF:
	case CTF_K_VOLATILE:
	case CTF_K_CONST:
	case CTF_K_RESTRICT:
	  if (tp->ctt_type == type || tp->ctt_type == otype
	      || tp->ctt_type == prev)
	    {
	      ctf_dprintf ("type %ld cycle detected\n", otype);
	      return (ctf_set_errno (ofp, ECTF_CORRUPT));
	    }
	  prev = type;
	  type = tp->ctt_type;
	  break;
	default:
	  return type;
	}
    }

  return CTF_ERR;		/* errno is set for us.  */
}

/* Lookup the given type ID and print a string name for it into buf.  Return
   the actual number of bytes (not including \0) needed to format the name.  */

ssize_t
ctf_type_lname (ctf_file_t *fp, ctf_id_t type, char *buf, size_t len)
{
  ctf_decl_t cd;
  ctf_decl_node_t *cdp;
  ctf_decl_prec_t prec, lp, rp;
  int ptr, arr;
  uint32_t k;

  if (fp == NULL && type == CTF_ERR)
    return -1;		/* Simplify caller code by permitting CTF_ERR.  */

  ctf_decl_init (&cd, buf, len);
  ctf_decl_push (&cd, fp, type);

  if (cd.cd_err != 0)
    {
      ctf_decl_fini (&cd);
      return (ctf_set_errno (fp, cd.cd_err));
    }

  /* If the type graph's order conflicts with lexical precedence order
     for pointers or arrays, then we need to surround the declarations at
     the corresponding lexical precedence with parentheses.  This can
     result in either a parenthesized pointer (*) as in int (*)() or
     int (*)[], or in a parenthesized pointer and array as in int (*[])().  */

  ptr = cd.cd_order[CTF_PREC_POINTER] > CTF_PREC_POINTER;
  arr = cd.cd_order[CTF_PREC_ARRAY] > CTF_PREC_ARRAY;

  rp = arr ? CTF_PREC_ARRAY : ptr ? CTF_PREC_POINTER : -1;
  lp = ptr ? CTF_PREC_POINTER : arr ? CTF_PREC_ARRAY : -1;

  k = CTF_K_POINTER;		/* Avoid leading whitespace (see below).  */

  for (prec = CTF_PREC_BASE; prec < CTF_PREC_MAX; prec++)
    {
      for (cdp = ctf_list_next (&cd.cd_nodes[prec]);
	   cdp != NULL; cdp = ctf_list_next (cdp))
	{
	  ctf_file_t *rfp = fp;
	  const ctf_type_t *tp = ctf_lookup_by_id (&rfp, cdp->cd_type);
	  const char *name = ctf_strptr (rfp, tp->ctt_name);

	  if (k != CTF_K_POINTER && k != CTF_K_ARRAY)
	    ctf_decl_sprintf (&cd, " ");

	  if (lp == prec)
	    {
	      ctf_decl_sprintf (&cd, "(");
	      lp = -1;
	    }

	  switch (cdp->cd_kind)
	    {
	    case CTF_K_INTEGER:
	    case CTF_K_FLOAT:
	    case CTF_K_TYPEDEF:
	      ctf_decl_sprintf (&cd, "%s", name);
	      break;
	    case CTF_K_POINTER:
	      ctf_decl_sprintf (&cd, "*");
	      break;
	    case CTF_K_ARRAY:
	      ctf_decl_sprintf (&cd, "[%u]", cdp->cd_n);
	      break;
	    case CTF_K_FUNCTION:
	      ctf_decl_sprintf (&cd, "()");
	      break;
	    case CTF_K_STRUCT:
	    case CTF_K_FORWARD:
	      ctf_decl_sprintf (&cd, "struct %s", name);
	      break;
	    case CTF_K_UNION:
	      ctf_decl_sprintf (&cd, "union %s", name);
	      break;
	    case CTF_K_ENUM:
	      ctf_decl_sprintf (&cd, "enum %s", name);
	      break;
	    case CTF_K_VOLATILE:
	      ctf_decl_sprintf (&cd, "volatile");
	      break;
	    case CTF_K_CONST:
	      ctf_decl_sprintf (&cd, "const");
	      break;
	    case CTF_K_RESTRICT:
	      ctf_decl_sprintf (&cd, "restrict");
	      break;
	    }

	  k = cdp->cd_kind;
	}

      if (rp == prec)
	ctf_decl_sprintf (&cd, ")");
    }

  if (cd.cd_len >= len)
    (void) ctf_set_errno (fp, ECTF_NAMELEN);

  ctf_decl_fini (&cd);
  return cd.cd_len;
}

/* Lookup the given type ID and print a string name for it into buf.  If buf
   is too small, return NULL: the ECTF_NAMELEN error is set on 'fp' for us.  */

char *
ctf_type_name (ctf_file_t *fp, ctf_id_t type, char *buf, size_t len)
{
  ssize_t rv = ctf_type_lname (fp, type, buf, len);
  return (rv >= 0 && rv < len ? buf : NULL);
}

/* Resolve the type down to a base type node, and then return the size
   of the type storage in bytes.  */

ssize_t
ctf_type_size (ctf_file_t *fp, ctf_id_t type)
{
  const ctf_type_t *tp;
  ssize_t size;
  ctf_arinfo_t ar;

  if ((type = ctf_type_resolve (fp, type)) == CTF_ERR)
    return -1;			/* errno is set for us.  */

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return -1;			/* errno is set for us.  */

  switch (LCTF_INFO_KIND (fp, tp->ctt_info))
    {
    case CTF_K_POINTER:
      return fp->ctf_dmodel->ctd_pointer;

    case CTF_K_FUNCTION:
      return 0;		/* Function size is only known by symtab.  */

    case CTF_K_ENUM:
      return fp->ctf_dmodel->ctd_int;

    case CTF_K_ARRAY:
      /* ctf_add_array() does not directly encode the element size, but
	 requires the user to multiply to determine the element size.

	 If ctf_get_ctt_size() returns nonzero, then use the recorded
	 size instead.  */

      if ((size = ctf_get_ctt_size (fp, tp, NULL, NULL)) > 0)
	return size;

      if (ctf_array_info (fp, type, &ar) == CTF_ERR
	  || (size = ctf_type_size (fp, ar.ctr_contents)) == CTF_ERR)
	return -1;		/* errno is set for us.  */

      return size * ar.ctr_nelems;

    default:
      return (ctf_get_ctt_size (fp, tp, NULL, NULL));
    }
}

/* Resolve the type down to a base type node, and then return the alignment
   needed for the type storage in bytes.

   XXX may need arch-dependent attention.  */

ssize_t
ctf_type_align (ctf_file_t *fp, ctf_id_t type)
{
  const ctf_type_t *tp;
  ctf_arinfo_t r;

  if ((type = ctf_type_resolve (fp, type)) == CTF_ERR)
    return -1;			/* errno is set for us.  */

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return -1;			/* errno is set for us.  */

  switch (LCTF_INFO_KIND (fp, tp->ctt_info))
    {
    case CTF_K_POINTER:
    case CTF_K_FUNCTION:
      return fp->ctf_dmodel->ctd_pointer;

    case CTF_K_ARRAY:
      if (ctf_array_info (fp, type, &r) == CTF_ERR)
	return -1;		/* errno is set for us.  */
      return (ctf_type_align (fp, r.ctr_contents));

    case CTF_K_STRUCT:
    case CTF_K_UNION:
      {
	uint32_t n = LCTF_INFO_VLEN (fp, tp->ctt_info);
	ssize_t size, increment;
	size_t align = 0;
	const void *vmp;

	(void) ctf_get_ctt_size (fp, tp, &size, &increment);
	vmp = (unsigned char *) tp + increment;

	if (LCTF_INFO_KIND (fp, tp->ctt_info) == CTF_K_STRUCT)
	  n = MIN (n, 1);	/* Only use first member for structs.  */

	if (size < CTF_LSTRUCT_THRESH)
	  {
	    const ctf_member_t *mp = vmp;
	    for (; n != 0; n--, mp++)
	      {
		ssize_t am = ctf_type_align (fp, mp->ctm_type);
		align = MAX (align, am);
	      }
	  }
	else
	  {
	    const ctf_lmember_t *lmp = vmp;
	    for (; n != 0; n--, lmp++)
	      {
		ssize_t am = ctf_type_align (fp, lmp->ctlm_type);
		align = MAX (align, am);
	      }
	  }

	return align;
      }

    case CTF_K_ENUM:
      return fp->ctf_dmodel->ctd_int;

    default:
      return (ctf_get_ctt_size (fp, tp, NULL, NULL));
    }
}

/* Return the kind (CTF_K_* constant) for the specified type ID.  */

int
ctf_type_kind (ctf_file_t *fp, ctf_id_t type)
{
  const ctf_type_t *tp;

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  return (LCTF_INFO_KIND (fp, tp->ctt_info));
}

/* If the type is one that directly references another type (such as POINTER),
   then return the ID of the type to which it refers.  */

ctf_id_t
ctf_type_reference (ctf_file_t *fp, ctf_id_t type)
{
  ctf_file_t *ofp = fp;
  const ctf_type_t *tp;

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  switch (LCTF_INFO_KIND (fp, tp->ctt_info))
    {
    case CTF_K_POINTER:
    case CTF_K_TYPEDEF:
    case CTF_K_VOLATILE:
    case CTF_K_CONST:
    case CTF_K_RESTRICT:
      return tp->ctt_type;
    default:
      return (ctf_set_errno (ofp, ECTF_NOTREF));
    }
}

/* Find a pointer to type by looking in fp->ctf_ptrtab.  If we can't find a
   pointer to the given type, see if we can compute a pointer to the type
   resulting from resolving the type down to its base type and use that
   instead.  This helps with cases where the CTF data includes "struct foo *"
   but not "foo_t *" and the user accesses "foo_t *" in the debugger.

   XXX what about parent containers?  */

ctf_id_t
ctf_type_pointer (ctf_file_t *fp, ctf_id_t type)
{
  ctf_file_t *ofp = fp;
  ctf_id_t ntype;

  if (ctf_lookup_by_id (&fp, type) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  if ((ntype = fp->ctf_ptrtab[LCTF_TYPE_TO_INDEX (fp, type)]) != 0)
    return (LCTF_INDEX_TO_TYPE (fp, ntype, (fp->ctf_flags & LCTF_CHILD)));

  if ((type = ctf_type_resolve (fp, type)) == CTF_ERR)
    return (ctf_set_errno (ofp, ECTF_NOTYPE));

  if (ctf_lookup_by_id (&fp, type) == NULL)
    return (ctf_set_errno (ofp, ECTF_NOTYPE));

  if ((ntype = fp->ctf_ptrtab[LCTF_TYPE_TO_INDEX (fp, type)]) != 0)
    return (LCTF_INDEX_TO_TYPE (fp, ntype, (fp->ctf_flags & LCTF_CHILD)));

  return (ctf_set_errno (ofp, ECTF_NOTYPE));
}

/* Return the encoding for the specified INTEGER or FLOAT.  */

int
ctf_type_encoding (ctf_file_t *fp, ctf_id_t type, ctf_encoding_t *ep)
{
  ctf_file_t *ofp = fp;
  const ctf_type_t *tp;
  ssize_t increment;
  uint32_t data;

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  (void) ctf_get_ctt_size (fp, tp, NULL, &increment);

  switch (LCTF_INFO_KIND (fp, tp->ctt_info))
    {
    case CTF_K_INTEGER:
      data = *(const uint32_t *) ((uintptr_t) tp + increment);
      ep->cte_format = CTF_INT_ENCODING (data);
      ep->cte_offset = CTF_INT_OFFSET (data);
      ep->cte_bits = CTF_INT_BITS (data);
      break;
    case CTF_K_FLOAT:
      data = *(const uint32_t *) ((uintptr_t) tp + increment);
      ep->cte_format = CTF_FP_ENCODING (data);
      ep->cte_offset = CTF_FP_OFFSET (data);
      ep->cte_bits = CTF_FP_BITS (data);
      break;
    default:
      return (ctf_set_errno (ofp, ECTF_NOTINTFP));
    }

  return 0;
}

int
ctf_type_cmp (ctf_file_t *lfp, ctf_id_t ltype, ctf_file_t *rfp,
	      ctf_id_t rtype)
{
  int rval;

  if (ltype < rtype)
    rval = -1;
  else if (ltype > rtype)
    rval = 1;
  else
    rval = 0;

  if (lfp == rfp)
    return rval;

  if (LCTF_TYPE_ISPARENT (lfp, ltype) && lfp->ctf_parent != NULL)
    lfp = lfp->ctf_parent;

  if (LCTF_TYPE_ISPARENT (rfp, rtype) && rfp->ctf_parent != NULL)
    rfp = rfp->ctf_parent;

  if (lfp < rfp)
    return -1;

  if (lfp > rfp)
    return 1;

  return rval;
}

/* Return a boolean value indicating if two types are compatible.  This function
   returns true if the two types are the same, or if they (or their ultimate
   base type) have the same encoding properties, or (for structs / unions /
   enums / forward declarations) if they have the same name and (for structs /
   unions) member count.  */

int
ctf_type_compat (ctf_file_t *lfp, ctf_id_t ltype,
		 ctf_file_t *rfp, ctf_id_t rtype)
{
  const ctf_type_t *ltp, *rtp;
  ctf_encoding_t le, re;
  ctf_arinfo_t la, ra;
  uint32_t lkind, rkind;
  int same_names = 0;

  if (ctf_type_cmp (lfp, ltype, rfp, rtype) == 0)
    return 1;

  ltype = ctf_type_resolve (lfp, ltype);
  lkind = ctf_type_kind (lfp, ltype);

  rtype = ctf_type_resolve (rfp, rtype);
  rkind = ctf_type_kind (rfp, rtype);

  ltp = ctf_lookup_by_id (&lfp, ltype);
  rtp = ctf_lookup_by_id (&rfp, rtype);

  if (ltp != NULL && rtp != NULL)
    same_names = (strcmp (ctf_strptr (lfp, ltp->ctt_name),
			  ctf_strptr (rfp, rtp->ctt_name)) == 0);

  if (lkind != rkind)
    return 0;

  switch (lkind)
    {
    case CTF_K_INTEGER:
    case CTF_K_FLOAT:
      memset (&le, 0, sizeof (le));
      memset (&re, 0, sizeof (re));
      return (ctf_type_encoding (lfp, ltype, &le) == 0
	      && ctf_type_encoding (rfp, rtype, &re) == 0
	      && memcmp (&le, &re, sizeof (ctf_encoding_t)) == 0);
    case CTF_K_POINTER:
      return (ctf_type_compat (lfp, ctf_type_reference (lfp, ltype),
			       rfp, ctf_type_reference (rfp, rtype)));
    case CTF_K_ARRAY:
      return (ctf_array_info (lfp, ltype, &la) == 0
	      && ctf_array_info (rfp, rtype, &ra) == 0
	      && la.ctr_nelems == ra.ctr_nelems
	      && ctf_type_compat (lfp, la.ctr_contents, rfp, ra.ctr_contents)
	      && ctf_type_compat (lfp, la.ctr_index, rfp, ra.ctr_index));
    case CTF_K_STRUCT:
    case CTF_K_UNION:
      return (same_names && (ctf_type_size (lfp, ltype)
			     == ctf_type_size (rfp, rtype)));
    case CTF_K_ENUM:
    case CTF_K_FORWARD:
      return same_names;   /* No other checks required for these type kinds.  */
    default:
      return 0;		      /* Should not get here since we did a resolve.  */
    }
}

/* Return the type and offset for a given member of a STRUCT or UNION.  */

int
ctf_member_info (ctf_file_t *fp, ctf_id_t type, const char *name,
		 ctf_membinfo_t *mip)
{
  ctf_file_t *ofp = fp;
  const ctf_type_t *tp;
  ssize_t size, increment;
  uint32_t kind, n;

  if ((type = ctf_type_resolve (fp, type)) == CTF_ERR)
    return CTF_ERR;		/* errno is set for us.  */

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  (void) ctf_get_ctt_size (fp, tp, &size, &increment);
  kind = LCTF_INFO_KIND (fp, tp->ctt_info);

  if (kind != CTF_K_STRUCT && kind != CTF_K_UNION)
    return (ctf_set_errno (ofp, ECTF_NOTSOU));

  if (size < CTF_LSTRUCT_THRESH)
    {
      const ctf_member_t *mp = (const ctf_member_t *) ((uintptr_t) tp +
						       increment);

      for (n = LCTF_INFO_VLEN (fp, tp->ctt_info); n != 0; n--, mp++)
	{
	  if (strcmp (ctf_strptr (fp, mp->ctm_name), name) == 0)
	    {
	      mip->ctm_type = mp->ctm_type;
	      mip->ctm_offset = mp->ctm_offset;
	      return 0;
	    }
	}
    }
  else
    {
      const ctf_lmember_t *lmp = (const ctf_lmember_t *) ((uintptr_t) tp +
							  increment);

      for (n = LCTF_INFO_VLEN (fp, tp->ctt_info); n != 0; n--, lmp++)
	{
	  if (strcmp (ctf_strptr (fp, lmp->ctlm_name), name) == 0)
	    {
	      mip->ctm_type = lmp->ctlm_type;
	      mip->ctm_offset = (unsigned long) CTF_LMEM_OFFSET (lmp);
	      return 0;
	    }
	}
    }

  return (ctf_set_errno (ofp, ECTF_NOMEMBNAM));
}

/* Return the array type, index, and size information for the specified ARRAY.  */

int
ctf_array_info (ctf_file_t *fp, ctf_id_t type, ctf_arinfo_t *arp)
{
  ctf_file_t *ofp = fp;
  const ctf_type_t *tp;
  const ctf_array_t *ap;
  ssize_t increment;

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  if (LCTF_INFO_KIND (fp, tp->ctt_info) != CTF_K_ARRAY)
    return (ctf_set_errno (ofp, ECTF_NOTARRAY));

  (void) ctf_get_ctt_size (fp, tp, NULL, &increment);

  ap = (const ctf_array_t *) ((uintptr_t) tp + increment);
  arp->ctr_contents = ap->cta_contents;
  arp->ctr_index = ap->cta_index;
  arp->ctr_nelems = ap->cta_nelems;

  return 0;
}

/* Convert the specified value to the corresponding enum tag name, if a
   matching name can be found.  Otherwise NULL is returned.  */

const char *
ctf_enum_name (ctf_file_t *fp, ctf_id_t type, int value)
{
  ctf_file_t *ofp = fp;
  const ctf_type_t *tp;
  const ctf_enum_t *ep;
  ssize_t increment;
  uint32_t n;

  if ((type = ctf_type_resolve (fp, type)) == CTF_ERR)
    return NULL;		/* errno is set for us.  */

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return NULL;		/* errno is set for us.  */

  if (LCTF_INFO_KIND (fp, tp->ctt_info) != CTF_K_ENUM)
    {
      (void) ctf_set_errno (ofp, ECTF_NOTENUM);
      return NULL;
    }

  (void) ctf_get_ctt_size (fp, tp, NULL, &increment);

  ep = (const ctf_enum_t *) ((uintptr_t) tp + increment);

  for (n = LCTF_INFO_VLEN (fp, tp->ctt_info); n != 0; n--, ep++)
    {
      if (ep->cte_value == value)
	return (ctf_strptr (fp, ep->cte_name));
    }

  (void) ctf_set_errno (ofp, ECTF_NOENUMNAM);
  return NULL;
}

/* Convert the specified enum tag name to the corresponding value, if a
   matching name can be found.  Otherwise CTF_ERR is returned.  */

int
ctf_enum_value (ctf_file_t * fp, ctf_id_t type, const char *name, int *valp)
{
  ctf_file_t *ofp = fp;
  const ctf_type_t *tp;
  const ctf_enum_t *ep;
  ssize_t size, increment;
  uint32_t n;

  if ((type = ctf_type_resolve (fp, type)) == CTF_ERR)
    return CTF_ERR;		/* errno is set for us.  */

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  if (LCTF_INFO_KIND (fp, tp->ctt_info) != CTF_K_ENUM)
    {
      (void) ctf_set_errno (ofp, ECTF_NOTENUM);
      return CTF_ERR;
    }

  (void) ctf_get_ctt_size (fp, tp, &size, &increment);

  ep = (const ctf_enum_t *) ((uintptr_t) tp + increment);

  for (n = LCTF_INFO_VLEN (fp, tp->ctt_info); n != 0; n--, ep++)
    {
      if (strcmp (ctf_strptr (fp, ep->cte_name), name) == 0)
	{
	  if (valp != NULL)
	    *valp = ep->cte_value;
	  return 0;
	}
    }

  (void) ctf_set_errno (ofp, ECTF_NOENUMNAM);
  return CTF_ERR;
}

/* Recursively visit the members of any type.  This function is used as the
   engine for ctf_type_visit, below.  We resolve the input type, recursively
   invoke ourself for each type member if the type is a struct or union, and
   then invoke the callback function on the current type.  If any callback
   returns non-zero, we abort and percolate the error code back up to the top.  */

static int
ctf_type_rvisit (ctf_file_t *fp, ctf_id_t type, ctf_visit_f *func,
		 void *arg, const char *name, unsigned long offset, int depth)
{
  ctf_id_t otype = type;
  const ctf_type_t *tp;
  ssize_t size, increment;
  uint32_t kind, n;
  int rc;

  if ((type = ctf_type_resolve (fp, type)) == CTF_ERR)
    return CTF_ERR;		/* errno is set for us.  */

  if ((tp = ctf_lookup_by_id (&fp, type)) == NULL)
    return CTF_ERR;		/* errno is set for us.  */

  if ((rc = func (name, otype, offset, depth, arg)) != 0)
    return rc;

  kind = LCTF_INFO_KIND (fp, tp->ctt_info);

  if (kind != CTF_K_STRUCT && kind != CTF_K_UNION)
    return 0;

  (void) ctf_get_ctt_size (fp, tp, &size, &increment);

  if (size < CTF_LSTRUCT_THRESH)
    {
      const ctf_member_t *mp = (const ctf_member_t *) ((uintptr_t) tp +
						       increment);

      for (n = LCTF_INFO_VLEN (fp, tp->ctt_info); n != 0; n--, mp++)
	{
	  if ((rc = ctf_type_rvisit (fp, mp->ctm_type,
				     func, arg, ctf_strptr (fp, mp->ctm_name),
				     offset + mp->ctm_offset,
				     depth + 1)) != 0)
	    return rc;
	}

    }
  else
    {
      const ctf_lmember_t *lmp = (const ctf_lmember_t *) ((uintptr_t) tp +
							  increment);

      for (n = LCTF_INFO_VLEN (fp, tp->ctt_info); n != 0; n--, lmp++)
	{
	  if ((rc = ctf_type_rvisit (fp, lmp->ctlm_type,
				     func, arg, ctf_strptr (fp,
							    lmp->ctlm_name),
				     offset + (unsigned long) CTF_LMEM_OFFSET (lmp),
				     depth + 1)) != 0)
	    return rc;
	}
    }

  return 0;
}

/* Recursively visit the members of any type.  We pass the name, member
 type, and offset of each member to the specified callback function.  */
int
ctf_type_visit (ctf_file_t *fp, ctf_id_t type, ctf_visit_f *func, void *arg)
{
  return (ctf_type_rvisit (fp, type, func, arg, "", 0, 0));
}
