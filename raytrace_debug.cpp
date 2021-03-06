/* raytrace.cpp
 * Ray tracer
 *
 * Mustafa Khafateh
 * CSC 473 Spring 2013
 */

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cmath>

#include <vector>


using namespace std;

#include "image.h"
#include "util.h"

#include "Camera.h"
#include "LightSource.h"
#include "Sphere.h"
#include "Plane.h"
#include "Box.h"
#include "Triangle.h"
#include "Ray.h"


// default values
int g_image_width = 640;
int g_image_height = 480;

Image g_image;
Image g_dbg_image;



vector<BaseObject*> theObjects;
vector<BaseObject*> g_lights;
vector<GeomObject*> g_geom;

Camera *g_camera;


void draw_circle(int r) {
    
    for (int y=0; y < g_image_height; y++) {
    	for (int x=0; x < g_image_width; x++) {
    	    
    	    if ((POWER2(x-g_image_width/2) + POWER2(y-g_image_height/2)) < 
    	         POWER2(r)) {
    	        g_image[x][y] = Color(0, ((y + 10) * 40) % 255, (y * 40) % 255);
    	    }
    	}
    }
}




void parse_pov(const string fname) {

    ifstream in;
    in.open(fname.c_str(), ifstream::in);

    if (!in.is_open()) {
        cerr << "can't open file: " << fname << endl;
        exit(1);
    }

    string word;
    BaseObject *read_obj;

    while (!in.eof()) {
        in >> word;

        if (in.eof()) {
            break;
        }
        // cout << "read '" << word << "'  eof: " << in.eof() << endl;

        if (word.size() > 0 && word[0] == '/') {
            // comment
            dprint("read comment");
            skip_line(in);
            continue;
        }

        read_obj = NULL;

        if (word == "camera") {
            dprint("read camera");
            g_camera = new Camera();
            read_obj = g_camera;
        } else if (word == "light_source") {
            read_obj = new LightSource();
            g_lights.push_back(read_obj);
        } else if (word == "box") {
            read_obj = new Box();
        } else if (word == "plane") {
            read_obj = new Plane();
        } else if (word == "triangle") {
            read_obj = new Triangle();
        } else if (word == "sphere") {
            read_obj = new Sphere();
        } else if (word == "cone") {
        }

        if (read_obj != NULL) {
            read_obj->read(in);
            theObjects.push_back(read_obj);

            if (dynamic_cast<GeomObject *>(read_obj)) {
                g_geom.push_back((GeomObject *) read_obj);
            }
        }
    }

    in.close();

}


// represents an intersection
struct Hit {
    Hit(float t, GeomObject *obj) {
        this->t = t;
        this->obj = obj;
    }
    float t;
    GeomObject *obj;
};



Hit* find_closest_hit(vector<Hit*> &hits) {
    if (hits.empty()) {
        return NULL;
    }

    int smallestIndex = 0;
    
    for (int i=0; i < hits.size(); i++) {
        if (hits[i]->t < hits[smallestIndex]->t) {
            smallestIndex = i;
        }
    }
    return hits[smallestIndex];
}


bool blocked_light(vec3 pos, LightSource *light) {

    // TODO test with global obj (then there'll be no stack alloc/dealloc
	Ray ray;
	
	ray.d = light->location - pos;
	ray.p0 = pos;
	
	for (int i=0; i < g_geom.size(); i++) {
        if (g_geom[i]->intersect(ray) > 0.00001) {
            return true;
        }
	}
	return false;
}


vec3 (*calcLighting)(GeomObject *obj, vec3 N, vec3 pos, LightSource *light);


vec3 calcLightingGaussian(GeomObject *obj, vec3 N, vec3 pos, LightSource *light) {


    float NL;
    vec3 L;
    vec3 color;
    float specular;
    
    vec3 pigment3 = vec3(obj->pigment.color.x,
            obj->pigment.color.y, obj->pigment.color.z);
    
    if (blocked_light(pos, light)) {
        // shadow
        return vec3(0) + obj->finish.ambient; // this is temporary FIXME
    }

    L = glm::normalize(light->location - pos);
    NL = MAX(dot(N, L), 0);

    vec3 V = glm::normalize(g_camera->location - pos);

    float smoothness = obj->finish.roughness == 0 ? 1 : 1/obj->finish.roughness;

    // specular
    if (NL > 0) {
        specular = exp(-pow(dot(V, vec3(2.0) * NL * N - L), 2) /
                smoothness);
    } else {
        specular = 0;
    }
	
	if (obj->name == "sphere") {
		// cout << "NL: " << NL << endl;
	}

    color = light->color *
            (vec3(specular) * vec3(obj->finish.specular) +
            vec3(NL) * pigment3) + obj->finish.ambient;

    return color;
}



vec3 calcLightingPhong(GeomObject *obj, vec3 N, vec3 pos, LightSource *light) {


    float NL;
    vec3 L;
    vec3 color;
    float specular;
    
    vec3 pigment3 = vec3(obj->pigment.color.x,
            obj->pigment.color.y, obj->pigment.color.z);
    
    if (blocked_light(pos, light)) {
        // shadow
        return vec3(0) + obj->finish.ambient; // this is temporary FIXME
    }

    L = glm::normalize(light->location - pos);
    NL = MAX(dot(N, L), 0);

    vec3 V = glm::normalize(g_camera->location - pos);

    float smoothness = obj->finish.roughness == 0 ? 1 : 1/obj->finish.roughness;

    // specular
    if (NL > 0) {
        specular = pow(max(dot(V, vec3(2.0) * NL * N - L), 0), smoothness);
    } else {
        specular = 0;
    }

    Plane *plane;
    if ((plane = dynamic_cast<Plane *>(obj))) {
        pigment3 = plane->getColor(pos);
    }
	
    color = light->color
            * (vec3(specular) * vec3(obj->finish.specular)
            + vec3(NL) * pigment3) + obj->finish.ambient;

    return color;
}


void add_epsilon(Ray &ray) {
    ray.p0 = ray.p0 + ray.d * vec3(0.0001);
}

void reflect_ray(Ray &ray, vec3 N, vec3 pos) {
    ray.p0 = pos;
    ray.d =  ray.d - vec3(2.0) * dot(N, ray.d) * N;
}

// psuedo code from Shirley's book, p. 214
bool refract_ray(const Ray &ray, const vec3 N, Ray &t, float n_over_n1, vec3 pos) {

    vec3 norm_d = normalize(ray.d);
    vec3 norm_n = normalize(N);

    float under_sqrt = 1 - POWER2(n_over_n1) * (1 - POWER2(dot(norm_d, norm_n)));

    vec3 first_term;

    if (under_sqrt < 0) {
        // total internal reflect
        return false;
    }

    first_term = n_over_n1 * (norm_d - norm_n * (dot(norm_d,norm_n)));

    t.d = first_term - norm_n * sqrt(under_sqrt);
    t.p0 = pos;

    add_epsilon(t);

    return true;
}


int num_initial_rays = 1;





vec3 cast_ray(Ray &ray, int recursion_depth=3) {
    Hit closest_hit(-1, NULL);

    // for lighting
    vec3 N;
    LightSource *light;
    vec3 pos;
    float t;
    
    Ray reflected_ray;


    // intersect ray with geometry
    for (int i=0; i < g_geom.size(); i++) {
        t = g_geom[i]->intersect(ray);
        if (t > 0 && (closest_hit.obj == NULL || t < closest_hit.t)) {
            closest_hit.t = t;
            closest_hit.obj = g_geom[i];
        }
    }

    

    vec3 final_color(0.0);
    Ray T;

    if (closest_hit.obj) {

        pos = ray.d * vec3(closest_hit.t) + ray.p0;
        N = closest_hit.obj->getNormal(pos);
        light = (LightSource *) g_lights[0]; // FIXME


        printf("---\n");
        printf("ray_cast()\nobj: %s\n", closest_hit.obj->name.c_str());
        print3f(pos, "pos");
        print3f(ray.d, "ray.d");

        printf("in sphere? r = %f\n", glm::length(pos));


        if (recursion_depth <= 1 ) {
            final_color = calcLighting(closest_hit.obj, N, pos, light);
        } else {

            // refract
            if (closest_hit.obj->finish.refraction > 0) {
                float d_dot_n = dot(ray.d, N);
                if (d_dot_n < 0) {
                    // out of obj
                    refract_ray(ray, N, T, 1/closest_hit.obj->finish.ior);
                    final_color = cast_ray(T, recursion_depth-1) *
                        (closest_hit.obj->finish.refraction);
                } else {
                    // in obj0
                    cout << "in obj!\n";
                    if (refract_ray(ray, N, T, closest_hit.obj->finish.ior/1)) {
                        // not total internal reflect
                        return cast_ray(T, recursion_depth-1) *
                               (closest_hit.obj->finish.refraction);
                    } else {
                        return vec3(0);
                    }
                }
            }

            // reflection
            if (closest_hit.obj->finish.reflection > 0) {
                reflect_ray(ray, N, pos);
                add_epsilon(ray);
                final_color += closest_hit.obj->finish.reflection
                                  * cast_ray(ray, recursion_depth-1);
            }

            final_color += (1.0f - closest_hit.obj->finish.reflection -
                                closest_hit.obj->pigment.color.w )
                           * calcLighting(closest_hit.obj, N, pos, light);
            
        }
    } else {
        // background
        final_color = vec3(0);
    }

    return final_color;
}

void cast_rays() {
    Ray *ray;
    
    int x, y;
    x = g_image_width/2;
    y = g_image_height/2 - 10;

    cout << "x: " << x << endl;

    /*
    for (int y=0; y < g_image_height; y++) { 
        for (int x=0; x < g_image_width; x++) {
        */
            // ray = g_camera->genOrthoRay(x, y);
            ray = g_camera->genRay(x, y);

            g_image[x][y] = cast_ray(*ray);

            delete ray;

#if 0

        }
        printf("\r %.3f done", 100 * ((float) y) / g_image_height);
        cout.flush();
    }
#endif 

    cout << endl;
}

int main(int argc, char* argv[]) {

    if (argc != 4) {
        cerr << "usage: " << argv[0] << " width height input.pov\n";
        exit(1);
    }

    g_image_width = atoi(argv[1]); 
    g_image_height = atoi(argv[2]); 

    if (g_image_width <= 0 || g_image_height <= 0) {
        cerr << "Error: Width and height must be positive\n";
        exit(1);
    }

    calcLighting = calcLightingPhong;

    string fname(argv[3]);
    parse_pov(fname);

    // print objects
    for (int i=0; i<theObjects.size(); i++) {
        cout << endl << endl << theObjects[i]->name << ":\n";
        theObjects[i]->print_properties();
    }

    cout << "w: " << g_image_width << " h: " << g_image_height
         << " fname: " << fname << endl;

    g_image = init_image(g_image_width, g_image_height);

    g_camera->setImageDimention(g_image_width, g_image_height);

    g_dbg_image = init_image(400, 300);


    // the main thing
    cast_rays();

    // test
    // draw_circle(g_image_height/3);

    // works only with ppm now
    string outfile_name(fname + ".ppm");
	write_image(g_image, outfile_name);

    write_image(g_dbg_image, "dbg.ppm");
	

    return 0;
}
