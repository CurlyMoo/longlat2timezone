/*
	Copyright (C) 2013 - 2014 CurlyMo

	This file is part of pilight.

    pilight is free software: you can redistribute it and/or modify it under the 
	terms of the GNU General Public License as published by the Free Software 
	Foundation, either version 3 of the License, or (at your option) any later 
	version.

    pilight is distributed in the hope that it will be useful, but WITHOUT ANY 
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR 
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see	<http://www.gnu.org/licenses/>
	
	This timezone data is extracted from the tz_world shapefiles made available
	by Eric Muller et al. at http://efele.net/maps/tz/
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>

#include "json.h"

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

#define NRCOUNTRIES 	408
#define PRECISION		1

int ***tzcoords;
unsigned int tznrpolys[NRCOUNTRIES];
char tznames[NRCOUNTRIES][30];
int tzdatafilled = 0;

int fillTZData(void) {
	if(tzdatafilled == 1) {
		return EXIT_SUCCESS;
	}
	char *content = NULL;
	JsonNode *root = NULL;
	FILE *fp;
	size_t bytes;
	struct stat st;

	char tzdatafile[] = "tzdata.json";
	/* Read JSON tzdata file */
	if(!(fp = fopen(tzdatafile, "rb"))) {
		printf("cannot read tzdata file: %s\n", tzdatafile);
		return EXIT_FAILURE;
	}

	fstat(fileno(fp), &st);
	bytes = (size_t)st.st_size;

	if(!(content = calloc(bytes+1, sizeof(char)))) {
		printf("out of memory\n");
		return EXIT_FAILURE;
	}

	if(fread(content, sizeof(char), bytes, fp) == -1) {
		printf("cannot read tzdata file: %s\n", tzdatafile);
	}
	fclose(fp);

	/* Validate JSON and turn into JSON object */
	if(json_validate(content) == false) {
		printf("tzdata is not in a valid json format\n");
		free(content);
		return EXIT_FAILURE;
	}
	root = json_decode(content);

	JsonNode *alist = json_first_child(root);
	unsigned int i = 0, x = 0, y = 0;
	while(alist) {
		JsonNode *country = json_first_child(alist);
		while(country) {
			strcpy(tznames[i], country->key);
			if(!(tzcoords = realloc(tzcoords, sizeof(int **)*(i+1)))) {
				printf("out of memory\n");
			}
			tzcoords[i] = NULL;
			JsonNode *coords = json_first_child(country);
			x = 0;
			while(coords) {
				y = 0;
				if(!(tzcoords[i] = realloc(tzcoords[i], sizeof(int *)*(x+1)))) {
					printf("out of memory\n");
				}
				tzcoords[i][x] = NULL;
				if(!(tzcoords[i][x] = malloc(sizeof(int)*2))) {
					printf("out of memory\n");
				}
				JsonNode *lonlat = json_first_child(coords);
				while(lonlat) {
					tzcoords[i][x][y] = (int)lonlat->number_;
					y++;
					lonlat = lonlat->next;
				}
				x++;
				coords = coords->next;
			}
			tznrpolys[i] = x;
			i++;
			country = country->next;
		}
		alist = alist->next;
	}
	json_delete(root);
	free(content);
	tzdatafilled = 1;
	return EXIT_SUCCESS;
}

int datetime_gc(void) {
	int i = 0, a = 0;
	if(tzdatafilled) {
		for(i=0;i<NRCOUNTRIES;i++) {
			unsigned int n = tznrpolys[i];
			for(a=0;a<n;a++) {
				free(tzcoords[i][a]);
			}
			free(tzcoords[i]);
		}
		free(tzcoords);
	}
	return EXIT_SUCCESS;
}

char *latlong2tz(double longitude, double latitude) {
	if(fillTZData() == EXIT_FAILURE) {
		return NULL;
	}

	int i = 0, a = 0, margin = 1, inside = 0;
	char *tz = NULL;

	margin *= (int)pow(10, PRECISION);
	int x = (int)round(longitude*(int)pow(10, PRECISION));
	int y = (int)round(latitude*(int)pow(10, PRECISION));	
	
	while(!inside && margin < (5*(int)pow(10, PRECISION))) {
		for(i=0;i<NRCOUNTRIES;i++) {
			unsigned int n = tznrpolys[i];
			if(n > 0) {
				int p1x = 0;
				int p1y = 0;
				for(a=0;a<n;a++) {
					if(tzcoords[i][a][0] < p1x || ((int)p1x == 0)) {
						p1x = tzcoords[i][a][0];
					}
					if(tzcoords[i][a][1] < p1y && ((int)p1y == 0)) {
						p1y = tzcoords[i][a][1];
					}
				} 
				for(a=0;a<n+1;a++) {
					int p2x = tzcoords[i][a % (int)n][0];
					int p2y = tzcoords[i][a % (int)n][1];
					if((round(p2x)-margin < round(x) && round(p2x)+margin > round(x))
					   &&(round(p2y)-margin < round(y) && round(p2y)+margin > round(y))) {			   
						int xinters = 0;
						if(y > min(p1y, p2y)) {
							if(y <= max(p1y, p2y)) {
								if(x <= max(p1x, p2x)) {
									if(p1y != p2y) {
										xinters = (y-p1y)*(p2x-p1x)/(p2y-p1y)+p1x;
									}
									if(p1x == p2x || x <= xinters) {
										tz = tznames[i];
										inside = 1;
										break;
									}
								}
							}
						}
						p1x = p2x;
						p1y = p2y;
					}
				}
				if(inside == 1) {
					break;
				}
			}
		}
		margin /= (int)pow(10, PRECISION);
		margin++;
		margin *= (int)pow(10, PRECISION);
	}
	return tz;
}

int main(void) {
	printf("%s\n", latlong2tz(4.895167899999933, 52.3702157));
	printf("%s\n", latlong2tz(-74.0059731, 40.7143528));
	printf("%s\n", latlong2tz(116.40752599999996, 39.90403));
	printf("%s\n", latlong2tz(-21.89521000000002, 64.13533799999999));
	
	datetime_gc();
	return EXIT_SUCCESS;
}
