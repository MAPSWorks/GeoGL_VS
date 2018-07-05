#ifndef _SM3D_DOTS_H_
#define _SM3D_DOTS_H_

#include <map>

#include "tile.h"

struct Point {
	double lng;
	double lat;
};

bool operator<(Point o1, Point o2);
bool operator==(Point o1, Point o2);

struct Dot {
	double lng;
	double lat;
	uint64_t seen_at;
	int size;
	int zoom_x[MAX_ZOOM - MIN_ZOOM];
	int zoom_y[MAX_ZOOM - MIN_ZOOM];
};

class Dots {
public:
	uint64_t time_base;
	uint64_t time_start;
	std::map<Point, Dot> dots;
	void run();
	uint64_t time_diff(uint64_t seen_at, uint64_t current);
private:
	double speed = 100;
	Dot make_dot(double lng, double lat, uint64_t seen_at);
};

#endif
