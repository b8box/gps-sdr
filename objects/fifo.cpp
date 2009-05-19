/*----------------------------------------------------------------------------------------------*/
/*! \file fifo.h
//
// FILENAME: fifo.h
//
// DESCRIPTION: Implements member functions of the FIFO class.
//
// DEVELOPERS: Gregory W. Heckler (2003-2009)
//
// LICENSE TERMS: Copyright (c) Gregory W. Heckler 2009
//
// This file is part of the GPS Software Defined Radio (GPS-SDR)
//
// The GPS-SDR is free software; you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version. The GPS-SDR is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// Note:  Comments within this file follow a syntax that is compatible with
//        DOXYGEN and are utilized for automated document extraction
//
// Reference:
 */
/*----------------------------------------------------------------------------------------------*/

#include "fifo.h"

/*----------------------------------------------------------------------------------------------*/
void *FIFO_Thread(void *_arg)
{

	FIFO *aFIFO = pFIFO;

	aFIFO->Open();

	while(grun)
	{
		aFIFO->Import();
		aFIFO->IncExecTic();
	}

	pthread_exit(0);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void FIFO::Start()
{

	Start_Thread(FIFO_Thread, NULL);

	if(gopt.verbose)
		printf("FIFO thread started\n");
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void FIFO::SetScale(int32 _agc_scale)
{
	agc_scale = _agc_scale;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
FIFO::FIFO():Threaded_Object("FIFTASK")
{
	int32 lcv;

	/* Create the buffer */
	buff = new ms_packet[FIFO_DEPTH];

	memset(buff, 0x0, sizeof(ms_packet)*FIFO_DEPTH);

	head = &buff[0];
	tail = &buff[0];

	/* Create circular linked list */
	for(lcv = 0; lcv < FIFO_DEPTH-1; lcv++)
		buff[lcv].next = &buff[lcv+1];

	buff[FIFO_DEPTH-1].next = &buff[0];

	/* Buffer for the raw IF data */
	if_buff = new CPX[IF_SAMPS_MS];

	tic = overflw = count = 0;

	agc_scale = 2048;

	sem_init(&sem_full, NULL, 0);
	sem_init(&sem_empty, NULL, FIFO_DEPTH);

	if(gopt.verbose)
		printf("Creating FIFO\n");

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
FIFO::~FIFO()
{
	int32 lcv;

	sem_destroy(&sem_full);
	sem_destroy(&sem_empty);

	delete [] if_buff;
	delete [] buff;

	close(npipe);

	if(gopt.verbose)
		printf("Destructing FIFO\n");

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void kill_program(int _sig)
{
	grun = false;
	printf("Lost USRP-GPS!\n");
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void FIFO::Import()
{
	int32 lcv;
	char *p;
	int32 nbytes, bread, bytes_per_read, agc_scale_p = agc_scale;

	bytes_per_read = IF_SAMPS_MS*sizeof(CPX);

	/* Get data from pipe (1 ms) */
	nbytes = 0; p = (char *)&if_buff[0];
	while((nbytes < bytes_per_read) && grun)
	{
		bread = read(npipe, &p[nbytes], PIPE_BUF);
		if(bread >= 0)
			nbytes += bread;
	}

	IncStartTic();

	/* Add to the buff */
	if(count == 0)
	{
		init_agc(&if_buff[0], IF_SAMPS_MS, AGC_BITS, &agc_scale);
	}
	else
	{
		overflw = run_agc(&if_buff[0], IF_SAMPS_MS, AGC_BITS, &agc_scale);
		Enqueue();
	}

	IncStopTic();

	count++;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void FIFO::Enqueue()
{

	sem_wait(&sem_empty);

	memcpy(&head->data[0], &if_buff[0], SAMPS_MS*sizeof(CPX));
	head->count = count;
	head = head->next;

	sem_post(&sem_full);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void FIFO::Dequeue(ms_packet *p)
{

	sem_wait(&sem_full);

	memcpy(p, tail, sizeof(ms_packet));
	tail = tail->next;

	sem_post(&sem_empty);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void FIFO::Open()
{

	/* Open the USRP_Uno pipe to get IF data */
	if(gopt.verbose)
		printf("Opening GPS pipe.\n");

	npipe = open("/tmp/GPSPIPE", O_RDONLY);

	if(gopt.verbose)
		printf("GPS pipe open.\n");

}
/*----------------------------------------------------------------------------------------------*/

