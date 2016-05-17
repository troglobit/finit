/* Finit state machine
 *
 * Copyright (c) 2016  xxx
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef FINIT_SM_H_
#define FINIT_SM_H_

typedef enum {
	SM_BOOTSTRAP_STATE = 0,
	SM_RUNNING_STATE,
	SM_RUNLEVEL_CHANGE_STATE,
	SM_RUNLEVEL_WAIT_STOP_STATE,
} sm_state_t;

typedef struct sm {
	sm_state_t state;
	int newlevel;
} sm_t;

void  sm_init(sm_t *sm);
void  sm_step(sm_t *sm);
void  sm_set_runlevel(sm_t *sm, int newlevel);

#endif	/* FINIT_SM_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
