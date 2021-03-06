/* terrain.cpp
 * Ray tracer
 * 
 * testing raymarching of perlin noise
 *
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
#include "BVH.h"

#include "gnoise.h"

// default values
int g_image_width = 640;
int g_image_height = 480;
int g_octaves;

Image g_image;
Image sky_image;



vector<BaseObject*> theObjects;
vector<LightSource*> g_lights;
vector<GeomObject*> g_geom;

BVHNode *g_obj_tree;


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
            g_lights.push_back((LightSource*) read_obj);
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





Hit* find_closest_hit(const Ray &ray) {

    Hit * non_plane_hit = g_obj_tree->intersect(ray);

    // TODO - test for planes

    return non_plane_hit;
}


bool blocked_light(vec3 pos, LightSource *light) {

	Ray ray;
	
	ray.d = light->location - pos;
	ray.p0 = pos;
    

    // BVH
    Hit* hit = find_closest_hit(ray);
    if (hit) {
        if (hit->t > 0.00001 && hit->t < 1.0) {
            return true;
        }
    }
	
    /*
    // non bvh
    float t;
	for (unsigned int i=0; i < g_geom.size(); i++) {
        t = g_geom[i]->intersect(ray);
        // if between light and intersect pt.
        if (t > 1.00001 && t < 1.0) {
            return true;
        }
	} */


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
            vec3(NL) * pigment3);

    return color;
}



vec3 calcLightingPhong(GeomObject *obj, vec3 N, vec3 pos, LightSource *light) {


    float NL;
    vec3 L;
    vec3 color;
    float specular;
    
    vec3 pigment3 = vec3(obj->pigment.color.x,
            obj->pigment.color.y, obj->pigment.color.z);
    

    L = glm::normalize(light->location - pos);
    NL = MAX(dot(N, L), 0);

    vec3 V = glm::normalize(g_camera->location - pos);

    float smoothness = obj->finish.roughness == 0 ? 1 : 1/obj->finish.roughness;

    // specular
    if (NL > 0) {
        specular = pow(Max(dot(V, vec3(2.0) * NL * N - L), 0), smoothness);
    } else {
        specular = 0;
    }

    /*
    Plane *plane;
    if ((plane = dynamic_cast<Plane *>(obj))) {
        pigment3 = plane->getColor(pos);
    }
    */
	
    color = light->color * (vec3(specular) * vec3(obj->finish.specular)
            + vec3(NL) * pigment3 * obj->finish.diffuse);

    return color;
}

vec3 calcLighting_all(GeomObject *obj, vec3 N, vec3 pos) {

    vec3 color = vec3(0.0f);

    vec3 pigment3 = vec3(obj->pigment.color.x,
            obj->pigment.color.y, obj->pigment.color.z);

    color = obj->finish.ambient * pigment3;

    for (int i=0; i < (int) g_lights.size(); i++) {
        // FIXME - enable shadows and fix
        //if (!blocked_light(pos, g_lights[i])) {
            color += calcLighting(obj, N, pos, g_lights[i]);
        //}
    }
    
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
bool refract_ray(const Ray &ray, const vec3 &pos, const vec3 N, Ray &t, float n_over_n1) {

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


vec3 cast_ray(Ray &ray, int recursion_depth=6) {
    Hit closest_hit(-1, NULL);

    Hit *hit;

    // for lighting
    vec3 N;
    vec3 pos;
    
    Ray reflected_ray;

    vec3 final_color(0.0);


    // intersect ray with geometry

    hit = find_closest_hit(ray);

    if (hit) {
        // cout << "hit\n";
        closest_hit.t = hit->t;
        closest_hit.obj = hit->obj;
        delete hit;
    } else {
        return final_color;
    }

    /*
    // non bvh
    for (unsigned int i=0; i < g_geom.size(); i++) {
        t = g_geom[i]->intersect(ray);
        if (t > 0 && (closest_hit.obj == NULL || t < closest_hit.t)) {
            closest_hit.t = t;
            closest_hit.obj = g_geom[i];
        }
    }
    */


    Ray T;

    // ray trace!
    pos = ray.d * vec3(closest_hit.t) + ray.p0;
    N = closest_hit.obj->getNormal(pos);

    if (recursion_depth <= 1 ) {
        final_color = calcLighting_all(closest_hit.obj, N, pos);
    } else {

        // refract
        if (closest_hit.obj->finish.refraction > 0) {
            float d_dot_n = dot(ray.d, N);
            if (d_dot_n < 0) {
                // out of obj
                refract_ray(ray, pos, N, T, 1/closest_hit.obj->finish.ior);
                final_color = cast_ray(T, recursion_depth-1) *
                    (closest_hit.obj->finish.refraction);
            } else {
                // in obj0
                if (refract_ray(ray, pos, -N, T, closest_hit.obj->finish.ior/1)) {
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
                       * calcLighting_all(closest_hit.obj, N, pos);
        
    }

    return final_color;
}


// returns in range -1, 1 (not sure if incusive)
float randFloat() {
    return ((float) rand() / RAND_MAX);
}






// === ray marching functions ===

float fBm(vec3 point, float H, float lacunarity, float octaves);

float fractal(float x, float z) {
    return fBm(vec3(x/50, z/50, 0.5), -1, 2.0, 3);
}


float noise1(float x, float z) {
    // return 10 * gnoise(x/10, z/10, 10);
    return 10 * fBm(vec3(x/40, z/40, 10), -1, 2.0, g_octaves);
}

vec3 noise1_normal(float x, float z) {

    const float eps = 0.0001;

    // central difference method. from iquilezles.org
    return normalize(vec3(noise1(x-eps, z) - noise1(x+eps, z),
                          2.0f*eps,
                          noise1(x, z-eps) - noise1(x, z+eps) ));

    /*
    // uses derivatives
    TODO - doesn't work
    vec3 N;
    normal_gnoise(N, x/10, z/10, 0.5);
    return normalize(vec3(-N.x, -N.y, -1));
    */
}



// ==== sin function ===
float sin_func(float x, float z) {
    return // sin(x) * sin(z) +
        0.5 * sin(0.2 * 2 * M_PI * x) +
        cos(0.1 * 2 * M_PI * z);
}

#define TWOPI (2 * M_PI)
vec3 func_norm(float x, float z) {
    vec3 normal = // vec3(-sin(z) * cos(x), 1.0, -sin(x) * cos(z)) +
                  vec3(-0.2 *TWOPI * 0.5 * cos(0.2 * TWOPI * x), 1.0, 0) + 
                  vec3(0, 1.0, 0.1 * TWOPI * sin(0.1 * TWOPI * z))
                ;

    return glm::normalize(normal);
}
// ==== end sin function ===


// get components of Hex color
#define H2R(c)   (((c) >> 16) & 0xFF)/255.0
#define H2G(c)   (((c) >> 8) & 0xFF)/255.0
#define H2B(c)   ((c) & 0xFF)/255.0
#define H2_3f(c) H2R(c), H2G(c), H2B(c)


vec3 lerp(float t, vec3 v0, vec3 v1) {
    return v0 + (v1 - v0) * t;
}

vec3 terrainColor(const Ray &ray, float t) {
    const vec3 pos = ray.p0 + ray.d * t;
    vec3 N = noise1_normal(pos.x, pos.z);

    vec3 brown(140/255.0f, 92/255.0f, 3/255.0f);
    vec3 grey(135/255.0f);

    /*
    if (pos.y < WATER_HIGHT) {
        return vec3(0, 0, 1);
    }
    */

    vec3 color = lerp(0.5 + pos.y/20, brown, grey);

    // printf("y: %f\n", N.y);
    // print3f(color, "lerped color");

    if (N.y > 0.9 ) {
        // more flat
        if (pos.y < 0) 
            color = vec3(H2_3f(0x185615)); // green
        else 
            color = vec3(H2_3f(0xF1F1FF)); // snow
    }

    g_geom[0]->pigment.color = vec3_to_vec4(color, 0);

    // vec3 fog = lerp(t/100.0f, vec3(0.0f), vec3(0.5, 0.5, 0.5));
    
    return calcLighting_all(g_geom[0], N, pos);
}


vec3 sky_color(int x, int y) {

    int x1, y1;

    float W = 1022 - 400;

    x1 = (int) ( (((float) x)/g_image_width) * W + 400);
    y1 = (int) (400-400 * (((float) y)/g_image_height));

    return vec3(sky_image[x1][y1].r/255.0f,
                sky_image[x1][y1].g/255.0f,
                sky_image[x1][y1].b/255.0f);
}



#define WATER_HIGHT -0.5
bool ray_march_intersect(const Ray &ray, float &resT) {


    // from iquilezles.org terrain marching article 
    float delt = 0.1f;
    const float mint = 0.001f;
    const float maxt = 100.0f;

    float lh = 0.0f;
    float ly = 0.0f;

    for (float t = mint; t < maxt; t += delt) {
        const vec3 p = ray.p0 + ray.d*t;

        /*
        if (p.y < WATER_HIGHT) {
            resT = t;
            return true;
        }
        */

        float h = noise1( p.x, p.z );

        if (p.y < h)
        {
            // printf("x: %f, z: %f\n", p.x, p.z);
            resT = t - delt + delt *(lh-ly)/(p.y-ly-h+lh);
            return true;
        }
        delt = 0.1f * t;

        lh = h;
        ly = p.y;
    }
    return false;
}


vec3 march_ray(const Ray &ray, int x, int y) {

    float t;

    if (ray_march_intersect(ray, t)) {
        return terrainColor(ray, t);
    } else {
        return sky_color(x, y);
    }
}


void render(int samples_per_pixel) {
    Ray *ray;

    vec3 color = vec3(0.0);

    for (int y=0; y < g_image_height; y++) { 
        for (int x=0; x < g_image_width; x++) {

            color *= 0.0f;
            if (samples_per_pixel > 1) {
                for (int i=0; i < samples_per_pixel; i++) {

                    float dx = 0.5 * randFloat();
                    float dy = 0.5 * randFloat();

                    ray = g_camera->genRay(x+dx, y+dy);

                    color += march_ray(*ray, x, y);

                    delete ray;
                }
            } else {
                ray = g_camera->genRay(x, y);
                color = march_ray(*ray, x, y);
                delete ray;
            }

            // take average
            g_image[x][y] = (1.0f/samples_per_pixel) * color;
        }
        printf("\r %.3f done", 100 * ((float) y) / g_image_height);
        cout.flush();
    }

    cout << endl;
}

/* 
 * procedural fBm evaluated at point.
 * from Texturing & Modeling: A procedural approach 3ed. p 437
 *
 * H - fractal increment
 * lacunarity - gap between succ freqs
 * ocaves - number of freqs in the fractal
 *
 */

float fBm(vec3 point, float H, float lacunarity, float octaves) {

    double value, remainder;
    int i;

    value = 0.0;
    /* inner loop of fractal construction */
    float exponent = 2;

    for (i=0; i<octaves; i++) {
        value += gnoise( point ) * exponent; // pow( lacunarity, -H*i );
        point *= lacunarity;

        exponent *= 0.5;
    }
    remainder = octaves - (int)octaves;
    if ( remainder ) /* add in “octaves” remainder */
        /* ‘i’ and spatial freq. are preset in loop above */
        value += remainder * gnoise( point ) * pow( lacunarity, -H*i );
    return value;
}

void  draw_gradient_noise() {

    for (int y=0; y < g_image_height; y++)
    for (int x=0; x < g_image_width; x++) {
        g_image[x][y] = vec3(0.5 + 0.5 * gnoise(30 + ((float) x)/5,
                               ((float) y)/5 - 10,
                                10));

        /*
        float scale = 50;
        float val = fBm(vec3(((float) x)/scale,((float) y)/scale, 10),
                    0.5, 2.0f, 4);
        g_image[x][y] = vec3(0.5 + 0.5 * val);
        */
    }

}


int main(int argc, char* argv[]) {

    if (argc < 4) {
        cerr << "usage: " << argv[0] <<
            " width height light.pov [number of octaves. default is 5]\n";
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
    cout << "g_geom size: " << g_geom.size() << endl;


    // print objects
    /*
    for (unsigned int i=0; i<theObjects.size(); i++) {
        cout << endl << endl << theObjects[i]->name << ":\n";
        theObjects[i]->print_properties();
    }
    */

    cout << "w: " << g_image_width << " h: " << g_image_height
         << " fname: " << fname << endl;

    g_image = init_image(g_image_width, g_image_height);

    sky_image = load_image("clouds_flickr_cubagallery.bmp");

    g_camera->setImageDimention(g_image_width, g_image_height);

    // g_obj_tree = new BVHNode(g_geom);
    // g_obj_tree->print();

    // Anti aliasing
    int samples_per_pixel = 5; // Zoe wants 9

    if (argc > 4) {
        samples_per_pixel = atoi(argv[4]);
    }

    g_octaves = samples_per_pixel;

    // the main thing
    render(1);
 
    // draw_gradient_noise();



    // TODO - has a bug
    // write_image(sky_image, "sky.ppm");


    // image test
    // draw_circle(g_image_height/3);

    // works only with ppm now
    string outfile_name(fname + ".ppm");
	write_image(g_image, outfile_name);
	

    return 0;
}
