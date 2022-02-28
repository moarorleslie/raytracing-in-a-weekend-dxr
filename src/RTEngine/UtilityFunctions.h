#ifndef UTILITY_FUNCTIONS_H
#define UTILITY_FUNCTIONS_H

#include "stdafx.h"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>


inline double random_double() {
    // Returns a random real in [0,1).
    return rand() / (RAND_MAX + 1.0);
}

inline static double random_double(double min, double max) {
    // Returns a random real in [min,max).
    return min + (max - min) * random_double();
}


inline static XMFLOAT4 random() {
    return XMFLOAT4(random_double(), random_double(), random_double(), 1);
}

inline static XMFLOAT4 random(double min, double max) {
    // Returns a random real in [min,max).
    return XMFLOAT4(random_double(min, max), random_double(min, max), random_double(min, max), 1);
}

inline int random_int(int min, int max) {
    // Returns a random integer in [min,max].
    return static_cast<int>(random_double(min, max + 1));
}

double length_squared(XMFLOAT3 myVec) {
    double result = myVec.x * myVec.x + myVec.y * myVec.y + myVec.z * myVec.z;
    return result;
}

double length(XMFLOAT3 myVec) {
    return sqrt(length_squared(myVec));
}

inline static float getDistance(XMFLOAT3 pointA, XMFLOAT3 pointB) {
    XMFLOAT3 diff = XMFLOAT3(pointA.x - pointB.x, pointA.y - pointB.y, pointA.z - pointB.z);
    return length(diff);
}

#endif // !UTILITY_FUNCTIONS_H

