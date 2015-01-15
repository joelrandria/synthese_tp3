#include "Image.h"
#include "ImageIO.h"
#include "Mesh.h"
#include "MeshIO.h"
#include "Transform.h"

#include <vector>
#include <string>
#include <stdio.h>

gk::Mesh* _mesh;
std::vector<gk::Vec4> _triangleColors;

//! Ressources
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
  int i;
  int triangleCount;

  float colorDelta;

  _mesh = loadMesh("scene/geometry.obj");

  triangleCount = _mesh->triangleCount();
  colorDelta = (float)1 / triangleCount;

  for (i = 0; i < triangleCount; ++i)
    _triangleColors.push_back(gk::Vec4(i * colorDelta, i * colorDelta, 0, 1));
}
void deleteScene()
{
  delete _mesh;
}

//! Raytracing
bool intersection(const gk::Ray& ray, const gk::Mesh& mesh, unsigned int& triangleId, gk::Hit& hit)
{
  int i;
  int triangleCount;

  bool intersect;

  float rt;
  float ru;
  float rv;

  float minrt;

  minrt = -1;
  intersect = false;
  triangleCount = mesh.triangleCount();

  for (i = 0; i < triangleCount; ++i)
  {
    if (mesh.triangle(i).Intersect(ray, ray.tmax, rt, ru, rv))
    {
      if (minrt < 0)
	minrt = rt;

      if (rt <= minrt)
      {
	minrt = rt;
	triangleId = i;
	intersect = true;
      }
    }
  }

  return intersect;
}

gk::Vec4 raytrace(const gk::Ray& ray)
{
  gk::Hit hit;
  uint triangleId;

  if (intersection(ray, *_mesh, triangleId, hit))
    return (_triangleColors[triangleId]);

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

  gk::Ray rayWorld;
  float rayWorldLength;

  gk::Point rayImageOrigin;
  gk::Point rayImageDestination;
  gk::Point rayWorldOrigin;
  gk::Point rayWorldDestination;

  loadScene();

  outputImage = gk::createImage(outputImageWidth, outputImageHeight);

  cameraUp = gk::Vector(0, 1, 0);
  cameraPosition = gk::Point(-500, 1500, 1000);

  v = gk::LookAt(cameraPosition, gk::Point(0, 0, 0), cameraUp);
  p = gk::Perspective(60, outputImageWidth / outputImageHeight, 1, 1000);
  i = gk::Viewport(outputImageWidth, outputImageHeight);
  vpiInv = (i * p * v).inverse();

  rayImageOrigin.z = 0;
  rayImageDestination.z = 1;

  for (y = 0; y < outputImageHeight; ++y)
  {
    rayImageOrigin.y = y;
    rayImageDestination.y = y;

    for (x = 0; x < outputImageWidth; ++x)
    {
      rayImageOrigin.x = x;
      rayImageDestination.x = x;

      rayWorldOrigin = vpiInv(rayImageOrigin);
      rayWorldDestination = vpiInv(rayImageDestination);

      rayWorldLength = gk::Distance(rayWorldOrigin, rayWorldDestination);

      rayWorld = gk::Ray(rayWorldOrigin, gk::Vector(rayWorldOrigin, rayWorldDestination) / rayWorldLength);
      rayWorld.tmax = rayWorldLength * 2;

      outputImage->setPixel(x, y, raytrace(rayWorld));
    }
  }

  gk::ImageIO::writeImage("raytracing.png", outputImage);
  delete outputImage;

  deleteScene();

  return 0;
}
