/*
 * librm: a library for interfacing to real-mode code
 *
 * Michael Brown <mbrown@fensystems.co.uk>
 *
 */

#ifdef KEEP_IT_REAL
/* Build a null object under -DKEEP_IT_REAL */
#else

#include "stdint.h"
#include "stddef.h"
#include "string.h"
#include "basemem.h"
#include "relocate.h"
#include <gpxe/init.h>
#include "librm.h"

/*
 * This file provides functions for managing librm.
 *
 */

/* Current location of librm in base memory */
char *installed_librm = librm;

/* Whether or not we have base memory currently allocated for librm.
 * Note that we *can* have a working librm present in unallocated base
 * memory; this is the situation at startup for all real-mode
 * prefixes.
 */
static int allocated_librm = 0;

/*
 * Allocate space on the real-mode stack and copy data there.
 *
 */
uint16_t copy_to_rm_stack ( void *data, size_t size ) {
#ifdef DEBUG_LIBRM
	if ( inst_rm_stack.offset <= size ) {
		printf ( "librm: out of space in RM stack\n" );
		lockup();
	}
#endif
	inst_rm_stack.offset -= size;
	copy_to_real ( inst_rm_stack.segment, inst_rm_stack.offset,
		       data, size );
	return inst_rm_stack.offset;
};

/*
 * Deallocate space on the real-mode stack, optionally copying back
 * data.
 *
 */
void remove_from_rm_stack ( void *data, size_t size ) {
	if ( data ) {
		copy_from_real ( data,
				 inst_rm_stack.segment, inst_rm_stack.offset,
				 size );
	}
	inst_rm_stack.offset += size;
};

/*
 * Install librm to base memory
 *
 */
static void install_librm ( char *addr ) {
	librm_base = virt_to_phys ( addr );
	memcpy ( addr, librm, librm_size );
	installed_librm = addr;
}

/*
 * Uninstall librm from base memory.  This copies librm back to the
 * "master" copy, so that it can be reinstalled to a new location,
 * preserving the values for rm_ss and rm_sp from the old installed
 * copy.
 *
 * We deliberately leave the old copy intact and effectively installed
 * (apart from being in unallocated memory) so that we can use it for
 * any real-mode calls required when allocating memory for the new
 * copy, or for the real-mode exit path.
 */
static void uninstall_librm ( void ) {

	/* Copy installed librm back to master copy */
	memcpy ( librm, installed_librm, librm_size );

	/* Free but do not zero the base memory */
	if ( allocated_librm ) {
		free_base_memory ( installed_librm, librm_size );
		allocated_librm = 0;
	}
}

/*
 * If librm isn't installed (i.e. if we have librm, but weren't
 * entered via it), then install librm and a real-mode stack to a
 * fixed temporary location, just so that we can e.g. issue printf()
 *
 * [ If we were entered via librm, then the real_to_prot call will
 * have filled in librm_base. ]
 */
static void librm_init ( void ) {
	if ( ! librm_base ) {
		install_librm ( phys_to_virt ( 0x7c00 ) );
		inst_rm_stack.segment = 0x7c0;
		inst_rm_stack.offset = 0x1000;
	}
}

/*
 * librm_post_reloc gets called immediately after relocation.
 *
 */
static void librm_post_reloc ( void ) {
	/* Point installed_librm back at last known physical location.
	 */
	installed_librm = phys_to_virt ( librm_base );

	/* Allocate base memory for librm and place a copy there */
	if ( ! allocated_librm ) {
		char *new_librm = alloc_base_memory ( librm_size );
		uninstall_librm ();
		install_librm ( new_librm );
		allocated_librm = 1;
	}
}

INIT_FN ( INIT_LIBRM, librm_init, NULL, uninstall_librm );
POST_RELOC_FN ( POST_RELOC_LIBRM, librm_post_reloc );

/*
 * Wrapper for initialise() when librm is being used.  We have to
 * install a copy of librm to allocated base memory and return the
 * pointer to this new librm's entry point via es:di.
 *
 */
void initialise_via_librm ( struct i386_all_regs *ix86 ) {
	/* Hand off to initialise() */
	initialise ();

	/* Point es:di to new librm's entry point.  Fortunately, di is
	 * already set up by setup16, so all we need to do is point
	 * es:0000 to the start of the new librm.
	 */
	ix86->segs.es = librm_base >> 4;
}

/*
 * Increment lock count of librm
 *
 */
void lock_librm ( void ) {
	inst_librm_ref_count++;
}

/*
 * Decrement lock count of librm
 *
 */
void unlock_librm ( void ) {
#ifdef DEBUG_LIBRM
	if ( inst_librm_ref_count == 0 ) {
		printf ( "librm: ref count gone negative\n" );
		lockup();
	}
#endif
	inst_librm_ref_count--;
}

#endif /* KEEP_IT_REAL */
