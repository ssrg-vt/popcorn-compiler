/* ====================================================================
 * Copyright (c) 1999-2001 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/********************************************************************
 * Example program to show usage of the live mode routines
 * The decoder is initialized with live_initialize_decoder()
 * Blocks of samples are decoded by live_utt_decode_block()
 * To compile an excutable compile using
 * $(CC) -I. -Isrc -Llibutil/linux -Lsrc/linux main_live_example.c -lutil -ldecoder -lm
 * from the current directory 
 * Note the include directories (-I*) and the library directories (-L*)
 *
 ********************************************************************/

/*

 * SPEC version of sphinx3 "main_live_pretend.c"
 
 * The provided routines 
 * required too much I/O to be acceptable to SPEC CPU;
 * this version remedies that by reading an audio input
 * into a memory buffer, and processing it repeatedly
 * with different settings.

 * Conceptually, this could be part of a naive "search for the optimal
 * parameters" program, although no attempt is made to
 * determine an optimal quality/settings tradeoff.

 * j.henning 20 apr 2005
 */


#include <stdio.h>
#include "libutil/libutil.h"
#include "live.h"
#include "cmd_ln_args.h"
#include "kb.h"

extern kb_t *kb;  // so that we can tweak them from here

// These four kinds of beams are discussed in 
// 482.sphinx3/Docs/sphinx3-intro-CMU.html
#define MAXBEAMSETS 1000
#define HMMBEAM   0
#define PBEAM     1
#define WBEAM     2
#define SUBVQBEAM 3
double beams[MAXBEAMSETS][4];

#define MAXUTTS  10000
#define FILENAMESZ 512

int main (int argc, char *argv[])
{
    short  *samps[MAXUTTS];
    char   *uttid[MAXUTTS];
    int    uttsize[MAXUTTS];
    int    iutt,nutt;
    int    i, j, buflen, endutt, blksize, nhypwds, nsamp;
    char   *argsfile, *ctlfile, *indir;
    char   filename[FILENAMESZ]; 
    char   cepfile[FILENAMESZ+4]; //above plus ".raw"
    int    filesize;
    partialhyp_t *parthyp;
    FILE *fp, *sfp;

    int ib,nbeams;
    FILE *beamsfp;

    // Read beams.dat: the sets of beams to be applied to all utterances

    if ((beamsfp = fopen("beams.dat", "r")) == NULL) {
        E_FATAL("Can't find beams.dat\n");
    }
    ib=0;
    while (ib < MAXBEAMSETS &&
           fscanf(beamsfp, "%lg%lg%lg%lg", 
             &beams[ib][HMMBEAM],
             &beams[ib][PBEAM],
             &beams[ib][WBEAM],
             &beams[ib][SUBVQBEAM]) != EOF) {
        ib++;
    }  
    nbeams = ib;
    if (nbeams < 1) {
        E_FATAL("Not enough beams %d\n", nbeams);
    }
    fclose(beamsfp);
    E_INFO("Processing %d beamsets\n", nbeams);

    // Other initialization

    if (argc != 4) {
      argsfile = NULL;
      parse_args_file(argsfile);
      E_FATAL("\nUSAGE: %s <ctlfile> <inrawdir> <argsfile>\n",argv[0]);
    }
    ctlfile = argv[1]; indir = argv[2]; argsfile = argv[3];
    blksize = 2000;
    if ((fp = fopen(ctlfile,"r")) == NULL)
        E_FATAL("Unable to read %s\n",ctlfile);
    live_initialize_decoder(argsfile);

    // Read in all utterances

    iutt = 0;
    while (iutt < MAXUTTS && fscanf(fp,"%s%d",filename,&filesize) != EOF) {
        if ((filesize<=0) || (filesize % sizeof(short) != 0)) {
            E_FATAL("Filesize claimed to be %d for %s\n", filesize, filename);
        }
        uttsize[iutt] = filesize/sizeof(short);
        if ((samps[iutt] = (short *) calloc(uttsize[iutt],sizeof(short))) == NULL) {
            E_FATAL("Can't allocate %d bytes for file %s\n", filesize, filename);
        }
        if ((uttid[iutt] = (char *) malloc(1 + strlen(filename))) == NULL) {
           E_FATAL("Can't allocate %d bytes for uttid %s", 1 + strlen(filename), filename);
        }
        strcpy(uttid[iutt],filename);
        sprintf(cepfile,"%s/%s.raw",indir,filename);
        if ((sfp = fopen(cepfile,"rb")) == NULL) {
            E_FATAL("Unable to read %s\n",cepfile);
        }
        nsamp = fread(samps[iutt], sizeof(short), uttsize[iutt], sfp);
        if (nsamp != uttsize[iutt]) {
            E_FATAL("Only read %d, expected %d in %s\n", nsamp, uttsize[iutt], filename);
        }
        fprintf(stdout,"%d samples in %s will be decoded in blocks of %d\n",
            nsamp,cepfile,blksize);
        fflush(stdout); fclose(sfp);
        iutt++;
    }
    nutt = iutt;

    // Now apply the beams to each of the utterances in turn

    for (ib=0; ib<nbeams; ib++) {
        kb->beam->hmm    = logs3 (beams[ib][HMMBEAM]);
        kb->beam->ptrans = logs3 (beams[ib][PBEAM]);
        kb->beam->word   = logs3 (beams[ib][WBEAM]);
        kb->beam->subvq  = logs3 (beams[ib][SUBVQBEAM]);
        E_INFO("Beam= %d, PBeam= %d, WBeam= %d, SVQBeam= %d\n",
            kb->beam->hmm, kb->beam->ptrans, kb->beam->word, kb->beam->subvq);

        for (iutt=0; iutt<nutt; iutt++) {
            live_utt_set_uttid(uttid[iutt]);
            for (i=0;i<uttsize[iutt];i+=blksize){
                buflen = i+blksize < uttsize[iutt] ? blksize : uttsize[iutt]-i;
                endutt = i+blksize <= uttsize[iutt]-1 ? 0 : 1;
                nhypwds = live_utt_decode_block(
                           samps[iutt]+i,buflen,endutt,&parthyp);

                //SPEC:uncomment for useful diags E_INFO("PARTIAL HYP:");
                //if (nhypwds > 0)
                //    for (j=0; j < nhypwds; j++) fprintf(stderr," %s",parthyp[j].word);
                //fprintf(stderr,"\n");
            }
        }
    }

    live_utt_summary();
    return 0;
}
