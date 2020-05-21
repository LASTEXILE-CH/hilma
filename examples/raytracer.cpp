#include <string>
#include <sstream>
#include <unistd.h>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/norm.hpp"

#include "hilma/math.h"
#include "hilma/text.h"
#include "hilma/timer.h"

#include "hilma/types/Camera.h"
#include "hilma/types/Image.h"

#include "hilma/ops/convert.h"
#include "hilma/ops/generate.h"
#include "hilma/ops/transform.h"
#include "hilma/ops/raytrace.h"
#include "hilma/ops/intersection.h"

#include "hilma/io/obj.h"
#include "hilma/io/ply.h"
#include "hilma/io/png.h"

using namespace hilma;

const float infinity = std::numeric_limits<float>::infinity();

glm::vec3 raytrace(const Ray& _ray, const std::vector<Hittable>& _hittables, int _depth) {
    if (_depth <= 0)
        return glm::vec3(0.0f);

    HitRecord rec;
    if ( raytrace(_ray, 0.001, infinity, _hittables, rec) ) {

        if (!rec.frontFace)
            return glm::vec3(0.0f);

        if (rec.line != nullptr)
            return glm::vec3(2.0f);

        glm::vec3 attenuation = glm::vec3(0.5f);
        glm::vec3 emission = glm::vec3(0.0f);
        glm::vec3 target = rec.normal;
        glm::vec3 lambert = random_unit_vector();

        if (rec.triangle->material != nullptr) {
            attenuation = rec.triangle->material->diffuse;
            emission = rec.triangle->material->emissive;
            glm::vec3 reflected = glm::reflect(glm::normalize(_ray.getDirection()), rec.normal);
            target = glm::mix(target, reflected, rec.triangle->material->metallic);
            target += lambert * (0.25f + rec.triangle->material->roughness);
        }
        else
            target += lambert;

        Ray scattered(rec.position, target);
        return emission + attenuation * raytrace( scattered, _hittables, _depth-1 );
    }


    glm::vec3 unit_direction = normalize(_ray.getDirection() );
    float t = 0.5f * (unit_direction.y + 1.0f);
    return glm::mix(glm::vec3(1.0f), glm::vec3(0.5f, 0.7f, 1.0f), t) * 0.5f;
}

int main(int argc, char **argv) {

    // Set up conext
    const float aspect_ratio = 16.0f / 9.0f;
    const int image_width = 1024 * 0.5;
    const int image_height = static_cast<int>(image_width / aspect_ratio);
    const float samples_per_pixel = 1;
    const float over_samples = 1.0f/samples_per_pixel; 
    const int max_depth = 50;
    bool debug = false;

    // Scene
    glm::vec3 lookfrom(3.5f, 0.5f, 1.5f);
    glm::vec3 lookat(0.0f, 0.0f, 0.0f);
    glm::vec3 vup(0.0f, -1.0f, 0.0f);
    float dist_to_focus = glm::length(lookfrom-lookat);
    float aperture = 0.15;
    Camera cam = Camera(lookfrom, lookat, vup,
                        35.0f, 
                        aspect_ratio, aperture, dist_to_focus);

    std::vector<Hittable> scene;

    // Mesh cornell = loadObj("CornellBox.obj");
    // center(cornell);
    // translateY(cornell, 0.4f);
    // translateZ(cornell, -2.0f);
    // scene.push_back( Hittable(cornell) );

    // Mesh head = loadPly("head.ply");
    // center(head);
    // scale(head, 0.15f);
    // scene.push_back( Hittable(head, true) );

    // Mesh plane = hilma::plane(6.0f, 6.0f, 1, 1);
    // translateZ(plane, -0.6f);
    // rotateX(plane, -PI/2.0f);
    // scene.push_back( Hittable(plane) );

    Image heightmap;
    loadPng("gale.png", heightmap, 1);
    Mesh terrain = toTerrain(heightmap, 100.0, 0.05);
    scale(terrain, 2.0/heightmap.getWidth());
    // rotateX(terrain, -PI/2.0f);
    // translateY(terrain, -1.0f);
    scene.push_back( Hittable(terrain) );

    // Material metal = Material("metal");
    // metal.diffuse = glm::vec3(0.5);
    // metal.metallic = 1.0f;
    // metal.roughness = 0.0f;
    // metal.diffuse = glm::vec3(1.0f);

    // Material plastic = Material("plastic");
    // plastic.diffuse = glm::vec3(0.3,0.3,1.0);
    // plastic.roughness = 0.2;

    Mesh icosphere = hilma::icosphere(0.5f, 2);
    // icosphere.setMaterial(metal);
    scene.push_back( Hittable(icosphere, debug) );

    // Mesh cone = hilma::cone(0.5f, 1.f, 36, 1, 1);
    // // cone.setMaterial(plastic);
    // rotateX(cone, PI);
    // translateX(cone, -2.0f);
    // scene.push_back( Hittable(cone, debug) );

    // Mesh cylinder = hilma::cylinder(0.5f, 1.f, 36, 1, 1, true);
    // // cylinder.setMaterial(metal);
    // translateX(cylinder, 2.0f);
    // scene.push_back( Hittable(cylinder, debug) );

    Timer timer;
    timer.start();
    std::cout << std::endl;
    
    const int totalPixels = image_width * image_height;
    const float totalRays = image_width * image_height * samples_per_pixel;
    Image image = Image(image_width, image_height, 3);

    for (int y = 0; y < image_height; ++y) {
        for (int x = 0; x < image_width; ++x) {
            int i = y * image_width + x;

            glm::vec3 pixel_color(0.0f, 0.0f, 0.0f);
            for (int s = 0; s < samples_per_pixel; ++s) {
                float u = (x + randomf()) / (image_width-1);
                float v = (y + randomf()) / (image_height-1);

                Ray ray = cam.getRay(u, v);
                pixel_color += raytrace(ray, scene, max_depth);
            }

            pixel_color = pixel_color * over_samples;
            pixel_color = sqrt(pixel_color);
            image.setColor(x, y, pixel_color);
            
            printProgressBar("RayTracing -", i / float(totalPixels), 100 );

        }
    }

    timer.stop();

    const float time_raycasting = timer.get() / 1000.f;
    std::cout << "                            Rendertime time : " << time_raycasting << " secs" << std::endl;

    int totalTriangles = 0;
    for (size_t i = 0; i < scene.size(); i++)
        totalTriangles += scene[i].triangles.size();    
    std::cout << "                  Total number of triangles : " << totalTriangles << std::endl;

    std::cout << "              Total number of ray-box tests : " << getTotalRayBoundingBoxTests() << std::endl;
    std::cout << "        Total number of ray-triangles tests : " << getTotalRayTriangleTests() << std::endl; 
    std::cout << "Total number of ray-triangles intersections : " << getTotalRayTrianglesIntersections() << std::endl;
    std::cout << "            Total number of ray-lines tests : " << getTotalLineLineTests() << std::endl; 
    std::cout << "    Total number of ray-lines intersections : " << getTotalLineLineIntersections() << std::endl;

    savePng("raytracer.png", image);

    return 0;
}