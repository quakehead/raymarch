#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include "glm/glm.hpp"
#include <iostream>

using std::istream;
using glm::vec3;
using glm::vec4;

void read_vec3(istream &in, vec3 &v);
void read_vec4(istream &in, vec4 &v);
void skip_to(istream &in, char target_ch);
void skip_line(istream &in);


#define print3f(v, name) \
        printf("%s: %f %f %f\n", name, v.x, v.y, v.z);


#define RAYTRACE_DEBUG

#ifdef RAYTRACE_DEBUG
#define dprint(x) puts(x)
#else
#define dprint(x) ;
#endif


#endif