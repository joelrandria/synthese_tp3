#include "Image.h"
#include "ImageIO.h"
#include "Mesh.h"
#include "MeshIO.h"
#include "Transform.h"

#include <vector>
#include <string>
#include <stdio.h>



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
void loadScene(std::vector<gk::Mesh*>& meshes)
{
  meshes.push_back(loadMesh("scene/geometry.obj"));
}
void deleteScene(std::vector<gk::Mesh*>& meshes)
{
  uint i;

  for (i = 0; i < meshes.size(); ++i)
    delete meshes[i];
}

//! Raytracing
bool intersection(const gk::Ray& ray, const gk::Mesh& mesh, gk::Hit& hit)
{
  int i;
  int triangleCount;

  float rt;
  float ru;
  float rv;

  triangleCount = mesh.triangleCount();

  for (i = 0; i < triangleCount; ++i)
  {
    if (mesh.triangle(i).Intersect(ray, 10000, rt, ru, rv))
      return true;
  }

  return false;
}

gk::VecColor raytrace(const gk::Ray& ray, const std::vector<gk::Mesh*>& meshes)
{
  uint i;

  gk::Hit hit;

  for (i = 0; i < meshes.size(); ++i)
    if (intersection(ray, *meshes[i], hit))
      return (gk::VecColor(1, 1, 1));

  return gk::VecColor(0, 0, 0);
}

int main(int, char**)
{
  int x;
  int y;

  std::vector<gk::Mesh*> meshes;

  gk::Image* outputImage;
  const int outputImageWidth = 1400;
  const int outputImageHeight = 1000;

  gk::Vector cameraUp;
  gk::Point cameraPosition;

  gk::Transform v;
  gk::Transform p;
  gk::Transform i;
  gk::Transform vpiInv;

  gk::Ray rayWorld;
  gk::Point rayImageOrigin;
  gk::Point rayImageDestination;
  gk::Point rayWorldOrigin;
  gk::Point rayWorldDestination;

  loadScene(meshes);

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

      rayWorld = gk::Ray(rayWorldOrigin, rayWorldDestination);

      outputImage->setPixel(x, y, raytrace(rayWorld, meshes));
    }
  }

  gk::ImageIO::writeImage("raytracing.png", outputImage);
  delete outputImage;

  deleteScene(meshes);

  return 0;
}
