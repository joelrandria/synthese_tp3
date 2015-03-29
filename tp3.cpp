#include "Image.h"
#include "ImageIO.h"
#include "Mesh.h"
#include "MeshIO.h"
#include "Transform.h"

#include <vector>
#include <string>
#include <cstdio>

gk::Mesh* _mesh;
std::vector<gk::Vec4> _triangleColors;
std::vector<uint> _lights;

//! Resources
gk::Mesh* loadMesh(const std::string& filename)
{
  gk::Mesh* mesh;

  mesh = gk::MeshIO::readOBJ(filename);
  if (mesh == 0)
  {
    fprintf(stderr, "loadMesh(): Impossible de charger le mesh '%s'\r\n", filename.c_str());
    exit(-1);
  }

  return mesh;
}

void loadScene()
{
  uint i;
  uint triangleCount;

  float colorDelta;

  _mesh = loadMesh("synthese_tp3/scenes/geometry.obj");

  triangleCount = (uint)_mesh->triangleCount();
  colorDelta = 0.25f / triangleCount;

  for (i = 0; i < triangleCount; ++i)
  {
    _triangleColors.push_back(gk::Vec4(0.5f + (i * colorDelta), 0.5f + (i * colorDelta), 0, 1));

    if (!(_mesh->triangleMaterial(i).emission == gk::Vec4(0, 0, 0, 0)))
      _lights.push_back(i);
  }
}
void deleteScene()
{
  delete _mesh;
}

//! Raytracing
void pickLightSources(const int lightSampleCount, std::vector<gk::Point>& points)
{
  uint i;
  int j;

  gk::Point p;

  const uint lightCount = _lights.size();

  points.clear();

  for (i = 0; i < lightCount; ++i)
  {
    for (j = 0; j < lightSampleCount; ++j)
    {
      _mesh->triangle(_lights[i]).sampleUniform((float)rand() / RAND_MAX, (float)rand() / RAND_MAX, p);

      points.push_back(p);
    }
  }
}

bool intersect(const gk::Ray& ray, const gk::Mesh& mesh, gk::Hit& hit)
{
  int i;
  int triangleCount;

  bool intersect;
  gk::Hit firstHit;

  intersect = false;
  firstHit.t = HUGE_VAL;

  triangleCount = mesh.triangleCount();

  for (i = 0; i < triangleCount; ++i)
  {
    if (mesh.triangle(i).Intersect(ray, ray.tmax, hit.t, hit.u, hit.v))
    {
      if (hit.t <= firstHit.t)
      {
	firstHit = hit;
	firstHit.object_id = i;

	intersect = true;
      }
    }
  }

  hit = firstHit;

  return intersect;
}

bool visibles(gk::Point p1, gk::Point p2)
{
  gk::Ray ray;
  gk::Hit hit;
  gk::Vector offset;

  offset = gk::Normalize(p2 - p1) * RAY_EPSILON;

  p1 += offset;
  p2 -= offset;

  ray = gk::Ray(p1, p2 - p1);
  ray.tmax = 1;

  return !intersect(ray, *_mesh, hit);
}

gk::Vec4 incidentLight(const gk::Point& p, const gk::Vector& d, const float tmax = HUGE_VAL)
{
  gk::Ray ray;

  gk::Hit hit;
  gk::Point hitPoint;

  std::vector<gk::Point> lights;
  const int lightSampleCount = 4;

  uint l;
  uint lightCount;

  gk::Vector lightOffset;
  gk::Point offsetHitPoint;
  gk::Point offsetLight;

  float lightingFactor;

  ray = gk::Ray(p + d * RAY_EPSILON, d);
  ray.tmax = tmax;

  if (intersect(ray, *_mesh, hit))
  {
    hitPoint = ray(hit.t);

    pickLightSources(lightSampleCount, lights);

    lightingFactor = 0;
    lightCount = lights.size();

    for (l = 0; l < lightCount; ++l)
    {
      lightOffset = gk::Normalize(lights[l] - hitPoint) * RAY_EPSILON;

      offsetHitPoint = hitPoint + lightOffset;
      offsetLight = lights[l] - lightOffset;

      if (visibles(offsetHitPoint, offsetLight))
	++lightingFactor;
    }

    lightingFactor /= lightCount;

    return gk::Vec4(_triangleColors[hit.object_id].x * lightingFactor,
		    _triangleColors[hit.object_id].y * lightingFactor,
		    _triangleColors[hit.object_id].z * lightingFactor,
		    1);
  }

  return gk::Vec4(0, 0, 0, 1);
}

int main(int, char**)
{
  int x;
  int y;

  gk::Image* outputImage;
  const int outputImageWidth = 1024;
  const int outputImageHeight = 768;

  gk::Vector cameraUp;
  gk::Point cameraPosition;

  gk::Transform v;
  gk::Transform p;
  gk::Transform i;
  gk::Transform vpiInv;

  gk::Point imageOrigin;
  gk::Point imageDestination;

  gk::Point worldOrigin;
  gk::Vector worldRay;

  srand(time(0));

  loadScene();

  outputImage = gk::createImage(outputImageWidth, outputImageHeight);

  cameraUp = gk::Vector(0, 1, 0);
  cameraPosition = gk::Point(-250, 750, 500);

  v = gk::LookAt(cameraPosition, gk::Point(0, 0, 0), cameraUp);
  p = gk::Perspective(60, outputImageWidth / outputImageHeight, 1, 4000);
  i = gk::Viewport(outputImageWidth, outputImageHeight);
  vpiInv = (i * p * v).inverse();

  imageOrigin.z = 0;
  imageDestination.z = 1;

  for (y = 0; y < outputImageHeight; ++y)
  {
    imageOrigin.y = y;
    imageDestination.y = y;

    for (x = 0; x < outputImageWidth; ++x)
    {
      imageOrigin.x = x;
      imageDestination.x = x;

      worldOrigin = vpiInv(imageOrigin);
      worldRay = vpiInv(imageDestination) - worldOrigin;

      outputImage->setPixel(x, y, incidentLight(worldOrigin, gk::Normalize(worldRay), worldRay.Length()));
    }
  }

  gk::ImageIO::writeImage("out.png", outputImage);

  delete outputImage;
  deleteScene();

  return 0;
}
