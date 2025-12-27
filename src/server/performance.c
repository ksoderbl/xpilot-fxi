#include <math.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "xpconfig.h"
#include "debug.h"
#include "proto.h"
#include "global.h"

#include "server.h"

#define MEASURE_LOOPS 500

static int32_t counter = 0;

void measure_time(void);
double compute_mean(double array[], int32_t n);
double compute_std(double array[], double mean, int32_t n);
double compute_max(double array[], int32_t n);
double compute_min(double array[], int32_t n);
static struct timeval t1 =
	{ 0, 0 };
static struct timeval t_now =
	{ 0, 0 };
static struct timeval t2 =
	{ 0, 0 };
static double measure[MEASURE_LOOPS];
static double measure2[MEASURE_LOOPS];

void insert_measure(void)
{
	int32_t time1;
	if (NumPlayers != 0) {
		gettimeofday(&t_now, NULL);
		time1 = t_now.tv_usec - t1.tv_usec;
		if (time1 < 0)
			time1 += 1000000;
		measure2[counter] = time1;
		gettimeofday(&t1, NULL);
	}
}

void measure_time(void)
{
	if (NumPlayers != 0) {
		gettimeofday(&t2, NULL);
		measure[counter] = (double) (t2.tv_usec - t1.tv_usec);
		if (measure[counter] < 0)
			measure[counter] += 1000000;
		if (((counter + 1) % MEASURE_LOOPS) == 0) {
			int32_t i = 0;
			xpprintf("scheduling times:\n");
			for (i = 0; i < MEASURE_LOOPS; i++) {
				xpprintf("%.3f\n", measure2[i]);
			}
			xpprintf("mainloop times:\n");
			for (i = 0; i < MEASURE_LOOPS; i++) {
				xpprintf("%.3f\n", measure[i]);
			}
			xpprintf("mainloop time:\n");
			xpprintf(
					"mean:%.3f us,  std_dev:%.3f us,  min:%3.f us,  max:%3.f us\n",
					compute_mean(measure, MEASURE_LOOPS),
					compute_std(
							measure,
							compute_mean(measure,
									MEASURE_LOOPS),
							MEASURE_LOOPS),
					compute_min(measure, MEASURE_LOOPS),
					compute_max(measure, MEASURE_LOOPS));
			xpprintf("scheduling:\n");
			xpprintf(
					"mean:%.3f us,  std_dev:%.3f us,  min:%3.f us,  max:%3.f us\n",
					compute_mean(measure2, MEASURE_LOOPS
					),
					compute_std(
							measure,
							compute_mean(measure2,
									MEASURE_LOOPS),
							MEASURE_LOOPS),
					compute_min(measure2, MEASURE_LOOPS),
					compute_max(measure2, MEASURE_LOOPS));

			xpprintf("measured for %d frames\n", MEASURE_LOOPS);
			End_game();
		}
		counter++;
	}
}

double compute_mean(double array[], int32_t n)
{
	int32_t i;
	double average = 0;
	for (i = 0; i < n; i++) {
		average += array[i];
	}
	average /= n;
	return average;
}

double compute_std(double array[], double mean, int32_t n)
{
	int32_t i;
	double variance = 0;
	for (i = 0; i < n; i++) {
		variance += (array[i] - mean) * (array[i] - mean);
	}
	variance /= n - 1;
	return sqrt(variance);
}

double compute_max(double array[], int32_t n)
{
	int32_t i;
	double element;
	element = array[0];
	for (i = 0; i < n; i++) {
		if (element < array[i])
			element = array[i];
	}
	return element;
}

double compute_min(double array[], int32_t n)
{
	int32_t i;
	double element;
	element = array[0];
	for (i = 0; i < n; i++) {
		if (element > array[i])
			element = array[i];
	}
	return element;
}
