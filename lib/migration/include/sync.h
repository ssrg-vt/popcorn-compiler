/*
 * sync.h
 * Copyright (C) 2018 Ho-Ren(Jack) Chuang <horenc@vt.edu>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef SYNC_H
#define SYNC_H

int popcorn_tso_begin(void);
int popcorn_tso_fence(void);
int popcorn_tso_end(void);

int popcorn_tso_begin_manual(void);
int popcorn_tso_fence_manual(void);
int popcorn_tso_end_manual(void);

#endif /* !SYNC_H */
