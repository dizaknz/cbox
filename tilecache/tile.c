#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SPHERE_MERCATOR 20037508.34
#define SPHERE_MERCATOR_GROUND_SIZE (SPHERE_MERCATOR*2)
#define GRID_ORIG_X (-1. * SPHERE_MERCATOR)
#define GRID_ORIG_Y (+1. * SPHERE_MERCATOR)

double mercX2lon (double x) {
  return x / SPHERE_MERCATOR * 180.;
}

double mercY2lat (double y) {
  double l = y / SPHERE_MERCATOR * 180.;

  return 180. / M_PI * ( 2. * atan( exp ( l * M_PI / 180 ) ) - M_PI / 2 );
}

double lon2mercX(double lon) 
{ 
  return lon * SPHERE_MERCATOR / 180.;
}

double lat2mercY(double lat)
{ 
  double y = log( tan( (90. + lat ) * M_PI / 360.) ) / ( M_PI / 180.);

  return y * SPHERE_MERCATOR / 180.;
}

#if 0

A: [-SPHERE_MERCATOR,SPHERE_MERCATOR]

  A---------------------------------D
  |    .    .    .    .    .    .   |
  |.................................|
  |    .    .    .    .    .    .   |
  |.................................|
  |    .    .    .    .    .    .   |
  B---------------------------------C

#endif

int lon2tileX (double lon, int tilesz) {
  return (int)((lon2mercX(lon) - GRID_ORIG_X) / tilesz);
}

int lat2tileY (double lat, int tilesz) {
  return (int)((GRID_ORIG_Y - lat2mercY(lat)) / tilesz);
}

#define LON 174.745666503906
#define LAT -37.0027961730957

int main (int argc, char **argv) {
  int x, y, zoom;
  double zoomfactor, tilesize, xmin, xmax, ymin, ymax;

  if (argc != 4) {
    printf("Error: supply x,y,zoom\n");
    exit (1);
  }
  
  x = strtol(argv[1], NULL, 10);
  y = strtol(argv[2], NULL, 10);
  zoom = strtol(argv[3], NULL, 10);

  zoomfactor = 1 << zoom;
  
  tilesize = SPHERE_MERCATOR_GROUND_SIZE / zoomfactor;
  xmin = (x * tilesize) - SPHERE_MERCATOR;
  xmax = ((x + 1) * tilesize) - SPHERE_MERCATOR;
  ymin = SPHERE_MERCATOR - ((y + 1) * tilesize);
  ymax = SPHERE_MERCATOR - (y * tilesize);
  
  printf ("x=%d y=%d z=%d zf=%f\n", x, y, zoom, zoomfactor);
  printf ("SPHERE_MERCATOR_GROUND_SIZE=%f tilesize=%f\n", SPHERE_MERCATOR_GROUND_SIZE, tilesize);
  printf ("xmin=%f ymin=%f xmax=%f ymax=%f\n", xmin, ymin, xmax, ymax);

  printf ("lon=%f lat=%f\n", LON, LAT);

  printf ("mercatorX=%f mercatorY=%f\n", lon2mercX(LAT), lat2mercY(LON));
  printf ("lon=%f lat=%f\n", mercX2lon (lon2mercX(LAT)), mercY2lat (lat2mercY(LON)));

  printf ("x=%d y=%d\n", lon2tileX(LAT, tilesize), lat2tileY(LON, tilesize));

  printf ("M_PI=%f\n", M_PI);

  return 0;
}
