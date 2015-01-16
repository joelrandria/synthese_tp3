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
std::vector<gk::Triangle> _lights;

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
  colorDelta = 0.25f / triangleCount;

  for (i = 0; i < triangleCount; ++i)
  {
    _triangleColors.push_back(gk::Vec4(0.5f + (i * colorDelta), 0.5f + (i * colorDelta), 0, 1));

    if (!(_mesh->triangleMaterial((uint)i).emission == gk::Vec4(0, 0, 0, 0)))
      _lights.push_back(_mesh->triangle((uint)i));
  }

}
void deleteScene()
{
  delete _mesh;
}

//! Raytracing
// gk::Point pickLightSource()
// {
  
// }

bool intersection(const gk::Ray& ray, const gk::Mesh& mesh, gk::Hit& hit)
{
  int i;
  int triangleCount;

  float minrt;
  bool intersect;

  minrt = -1;
  intersect = false;
  triangleCount = mesh.triangleCount();

  for (i = 0; i < triangleCount; ++i)
  {
    if (mesh.triangle(i).Intersect(ray, ray.tmax, hit.t, hit.u, hit.v))
    {
      if (minrt < 0)
	minrt = hit.t;

      if (hit.t <= minrt)
      {
	minrt = hit.t;
	hit.object_id = i;
	intersect = true;
      }
    }
  }

  return intersect;
}

gk::Vec4 incidentLight(const gk::Ray& ray)
{
  gk::Hit hit;
  gk::Point lightSource;

  if (intersection(ray, *_mesh, hit))
    return _triangleColors[hit.object_id];

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
  cameraPosition = gk::Point(-250, 750, 500);

  v = gk::LookAt(cameraPosition, gk::Point(0, 0, 0), cameraUp);
  p = gk::Perspective(60, outputImageWidth / outputImageHeight, 1, 4000);
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
      rayWorld.tmax = rayWorldLength;

      outputImage->setPixel(x, y, incidentLight(rayWorld));
    }
  }

  gk::ImageIO::writeImage("raytracing.png", outputImage);
  delete outputImage;

  deleteScene();

  return 0;
}
