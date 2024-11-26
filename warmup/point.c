#include <assert.h>
#include "common.h"
#include "point.h"
#include "math.h"

void
point_translate(struct point *p, double x, double y)
{
	//TBD();
        p -> x += x;
        p -> y += y;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
	//TBD();
	return pow(pow(p1 -> x - p2 -> x, 2) + pow(p1 -> y - p2 -> y, 2), 0.5);
}

int
point_compare(const struct point *p1, const struct point *p2)
{
        double p1d = pow(pow(p1 -> x, 2) + pow(p1 -> y, 2), 0.5);
        double p2d = pow(pow(p2 -> x, 2) + pow(p2 -> y, 2), 0.5);
        
        if(p1d < p2d)
            return -1;
        
        else if(p1d > p2d)
            return 1;
        
        else
            return 0;
}
