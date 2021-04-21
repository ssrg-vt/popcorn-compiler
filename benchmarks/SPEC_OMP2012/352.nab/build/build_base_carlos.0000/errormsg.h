/*
 *
 * This software is copyrighted, 1995, by Tom Macke and David A. Case. 
 * The following terms apply to all files associated with the software 
 * unless explicitly disclaimed in individual files.
 * 
 * The authors hereby grant permission to use, copy, modify, and re-distribute
 * this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be distributed provided that
 * the nature of the modifications are clearly indicated.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 * 
 */

#define	E_INTERNAL_ERROR	"Internal nab program error, exiting.\n"
#define	E_UNEXPECTED_SYM_D	"Unexpected symbol %d.\n"
#define	E_NOMEM_FOR_S		"Unable to allocate space for %s.\n"
#define	E_REDEF_BUILTIN_S	"Builtin symbol %s can't be redefined.\n"
#define	E_REDEF_SYM_S		"Redefinition of symbol %s.\n"
#define	E_NO_DECL_S		"Symbol %s not declared.\n"
#define	E_UNKNOWN_ATTR_S	"Unknown attrbute %s.\n"
#define	E_NOSUCH_STRAND_S	"Strand %s not in molecule.\n"
#define	E_NOSUCH_RESIDUE_S	"Residue %s.\n"
#define	E_NOSUCH_END_S		"Strand ends are \"first\", \"last\" not %s.\n"
#define	E_NOSUCH_ATOM_S		"Atom %s.\n"
#define	E_CANT_OPEN_RESLIB_S	"Can't open residue library %s.\n"
#define	E_CANT_OPEN_S		"Can't open file %s.\n"
#define	E_BAD_RESLIB_HEADER_S	"Incorrect line in residue library header %s...\n"
#define	E_BAD_BNDFILE_HEADER_S	"Incorrect header line in bond file: %s...\n"
#define	E_BAD_BNDFILE_DATA_S	"Incorrect data line in bond file: %s...\n"
#define	E_LIGATE_BAD_ENDS_S	"end1/end2 in ligate() must be be 5'/3' or 3'/5 not %s\n"
#define	E_MISSING_FIELDS_S	"First use of struct tag %s requires field list\n"
#define	E_STRUCT_TAG_EXPECTED_S	"ID %s must be a struct tag\n"
#define	E_NOFIELDS_ALLOWED_S	"Only first decl of %s can be followed by fields\n"

void	errormsg( int, char* );
void	errormsg_s( int, char*, char* );
void	errormsg_2s( int, char*, char*, char* );
void	errormsg_d( int, char*, int );
