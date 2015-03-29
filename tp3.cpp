#include "Image.h"
#include "ImageIO.h"
#include "Mesh.h"
#include "MeshIO.h"
#include "Transform.h"

#include <vector>
#include <string>
#include <cstdio>

//! Global settings
const int _outputImageWidth = 1024;
const int _outputImageHeight = 768;
const char* _outputImageFilename = "out.png";

const int _directLightingSampleCount = 4;

//! Geometry
gk::Mesh* _mesh;
std::vector<gk::Vec4> _triangleColors;

//! Lights
std::vector<uint> _lights;
std::vector<float> _lightPdf;
std::vector<float> _lightCdf;

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

  uint lightCount;

  float partialLightAreaSum;
  float totalLightAreaSum;

  _mesh = loadMesh("synthese_tp3/scenes/geometry.obj");

  triangleCount = (uint)_mesh->triangleCount();
  colorDelta = 0.25f / triangleCount;

  // Triangle colors assignation & Lights processing
  totalLightAreaSum = 0;

  for (i = 0; i < triangleCount; ++i)
  {
    _triangleColors.push_back(gk::Vec4(0.5f + (i * colorDelta), 0.5f + (i * colorDelta), 0, 1));

    if (!(_mesh->triangleMaterial(i).emission == gk::Vec4(0, 0, 0, 0)))
    {
      _lights.push_back(i);

      totalLightAreaSum += _mesh->triangle(i).area();
    }
  }

  // Light picking pdf & cdf initialization
  partialLightAreaSum = 0;

  lightCount = _lights.size();

  for (i = 0; i < lightCount; ++i)
  {
    partialLightAreaSum += _mesh->triangle(_lights[i]).area();

    _lightPdf.push_back(_mesh->triangle(_lights[i]).area() / totalLightAreaSum);
    _lightCdf.push_back(partialLightAreaSum / totalLightAreaSum);
  }

  printf("--------------- Light's PDF ---------------\r\n");
  for (i = 0; i < lightCount; ++i)
    printf("Light[%u] PDF = %f\r\n", i, _lightPdf[i]);
  printf("--------------- Light's CDF ---------------\r\n");
  for (i = 0; i < lightCount; ++i)
    printf("Light[%u] CDF = %f\r\n", i, _lightCdf[i]);
}
void deleteScene()
{
  delete _mesh;
}

//! Raytracing
float sampleLightPoint(gk::Point& p, gk::Normal& n)
{
  float pdf;

  float u;
  float v;

  uint i;
  uint light;

  float su;
  float sv;
  gk::PNTriangle pnt;

  // Uniform light picking
  u = (float)rand() / RAND_MAX;
  v = (float)rand() / RAND_MAX;

  for (i = 0; i < _lights.size(); ++i)
  {
    if (u < _lightCdf[i])
    {
      light = i;
      pdf = _lightPdf[i];

      break;
    }
  }

  // Uniform point picking within the selected light source
  pnt = _mesh->pntriangle(_lights[light]);

  pdf *= pnt.sampleUniformUV(u, v, su, sv);

  p = pnt.point(su, sv);
  n = pnt.normal(su, sv);

  return pdf;
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
  gk::Normal hitNormal;

  uint l;

  float lightPointPdf;
  gk::Point lightPoint;
  gk::Normal lightNormal;

  gk::Vector lightOffset;
  gk::Point offsetHitPoint;
  gk::Point offsetLight;

  gk::Vec4 lightSum;

  ray = gk::Ray(p + d * RAY_EPSILON, d);
  ray.tmax = tmax;

  if (intersect(ray, *_mesh, hit))
  {
    hitPoint = ray(hit.t);
    hitNormal = _mesh->pntriangle(hit.object_id).normal(hit.u, hit.v);

    // Eclairage direct
    lightSum = gk::Vec4(0, 0, 0, 0);

    for (l = 0; l < _directLightingSampleCount; ++l)
    {
      lightPointPdf = sampleLightPoint(lightPoint, lightNormal);

      // lightOffset = gk::Normalize(lights[l] - hitPoint) * RAY_EPSILON;

      // offsetLight = lights[l] - lightOffset;
      // offsetHitPoint = hitPoint + lightOffset;

      // if (visibles(offsetHitPoint, offsetLight))
      // {
	
      // }
    }

    // return gk::Vec4(_triangleColors[hit.object_id].x * lightingFactor,
    // 		    _triangleColors[hit.object_id].y * lightingFactor,
    // 		    _triangleColors[hit.object_id].z * lightingFactor,
    // 		    1);
  }

  return gk::Vec4(0, 0, 0, 1);
}

int main(int, char**)
{
  int x;
  int y;

  gk::Image* outputImage;

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

  outputImage = gk::createImage(_outputImageWidth, _outputImageHeight);

  cameraUp = gk::Vector(0, 1, 0);
  cameraPosition = gk::Point(-250, 750, 500);

  v = gk::LookAt(cameraPosition, gk::Point(0, 0, 0), cameraUp);
  p = gk::Perspective(60, _outputImageWidth / _outputImageHeight, 1, 4000);
  i = gk::Viewport(_outputImageWidth, _outputImageHeight);
  vpiInv = (i * p * v).inverse();

  imageOrigin.z = 0;
  imageDestination.z = 1;

  for (y = 0; y < _outputImageHeight; ++y)
  {
    imageOrigin.y = y;
    imageDestination.y = y;

    for (x = 0; x < _outputImageWidth; ++x)
    {
      imageOrigin.x = x;
      imageDestination.x = x;

      worldOrigin = vpiInv(imageOrigin);
      worldRay = vpiInv(imageDestination) - worldOrigin;

      outputImage->setPixel(x, y, incidentLight(worldOrigin, gk::Normalize(worldRay), worldRay.Length()));
    }
  }

  gk::ImageIO::writeImage(_outputImageFilename, outputImage);

  delete outputImage;
  deleteScene();

  return 0;
}
