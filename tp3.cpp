#include "Image.h"
#include "ImageIO.h"
#include "Mesh.h"
#include "MeshIO.h"
#include "Transform.h"

#include <vector>
#include <string>
#include <cstdio>

//! Global settings
const char* _inputFilename = "synthese_tp3/scenes/geometry.obj";

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

//! gKit utilities
gk::Vec4 operator+(const gk::Vec4& v1, const gk::Vec4& v2)
{
  return gk::Vec4(v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2], v1[3] + v2[3]);
}
gk::Vec4 operator+=(gk::Vec4& v1, const gk::Vec4& v2)
{
  v1 = v1 + v2;
  return v1;
}

gk::Vec4 operator*(const gk::Vec4& v1, const gk::Vec4& v2)
{
  return gk::Vec4(v1[0] * v2[0], v1[1] * v2[1], v1[2] * v2[2], v1[3] * v2[3]);
}
gk::Vec4 operator*(const gk::Vec4& v, const float f)
{
  return gk::Vec4(v[0] * f, v[1] * f, v[2] * f, v[3] * f);
}
gk::Vec4 operator*=(gk::Vec4& v, const float f)
{
  v = v * f;
  return v;
}

gk::Vec4 operator/(const gk::Vec4& v, const float f)
{
  return v * (1.f / f);
}
gk::Vec4 operator/=(gk::Vec4& v, const float f)
{
  v = v / f;
  return v;
}

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

  _mesh = loadMesh(_inputFilename);

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

//! Sampling
float uniformLightsSampling(gk::Point& p, gk::Normal& n, uint& triangleId)
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
      triangleId = _lights[i];

      break;
    }
  }

  // Uniform point picking within the selected light source
  pnt = _mesh->pntriangle(_lights[light]);

  pdf *= pnt.sampleUniformUV(u, v, su, sv);

  p = pnt.point(su, sv);
  n = ((gk::Triangle)pnt).normal();

  return pdf;
}
float uniformHemisphereSampling(const gk::Vector& n, gk::Vector& d)
{
  float r1;
  float r2;
  float squaredr2;

  gk::Vector t1;
  gk::Vector t2;
  gk::Transform t;

  // Unit aligned hemisphere uniform sampling
  r1 = (float)rand() / RAND_MAX;
  r2 = (float)rand() / RAND_MAX;

  squaredr2 = r2 * r2;

  d.x = cos(2 * M_PI * r1) * sqrt(1.f - squaredr2);
  d.y = r2;
  d.z = sin(2 * M_PI * r1) * sqrt(1.f - squaredr2);

  // Normal alignement
  gk::CoordinateSystem(n, &t1, &t2);

  t = gk::Transform
    (gk::Matrix4x4
     (t1.x, n.x, t2.x, 0,
      t1.y, n.y, t2.y, 0,
      t1.z, n.z, t2.z, 0,
      0,    0,    0,   1)
     .getInverse());

  d = t(d);

  return 1.f / (2 * M_PI);
}

//! Raytracing
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
  gk::MeshMaterial hitMaterial;

  uint l;

  float lightPointPdf;
  uint lightTriangleId;
  gk::Point lightPoint;
  gk::Normal lightNormal;

  gk::Vector lightDirection;
  gk::Vector lightOffset;
  gk::Point offsetHitPoint;
  gk::Point offsetLight;

  gk::Vector h;

  float cos_h;
  float cos_nl;
  float cos_nln;

  gk::Vec4 li;
  gk::Vec4 brdf;

  gk::Vec4 directLight;
  gk::Vec4 directLightSum;

  ray = gk::Ray(p + d * RAY_EPSILON, d);
  ray.tmax = tmax;

  if (intersect(ray, *_mesh, hit))
  {
    hitPoint = ray(hit.t);
    hitNormal = _mesh->triangle(hit.object_id).normal();

    // Direct lighting
    directLightSum = gk::Vec4(0, 0, 0, 0);

    for (l = 0; l < _directLightingSampleCount; ++l)
    {
      lightPointPdf = uniformLightsSampling(lightPoint, lightNormal, lightTriangleId);

      lightDirection = gk::Normalize(lightPoint - hitPoint);
      lightOffset = lightDirection * RAY_EPSILON;

      offsetLight = lightPoint - lightOffset;
      offsetHitPoint = hitPoint + lightOffset;

      directLight = gk::Vec4(0, 0, 0, 0);

      if (visibles(offsetHitPoint, offsetLight))
      {
	h = gk::Normalize((lightDirection - ray.d) * .5f);

	cos_h = fabs(gk::Dot(hitNormal, h));
	cos_nl = fabs(gk::Dot(hitNormal, lightDirection));
	cos_nln = fabs(gk::Dot(lightNormal, lightDirection));

	hitMaterial =  _mesh->triangleMaterial(hit.object_id);

	li = _mesh->triangleMaterial(lightTriangleId).emission;
	brdf = (hitMaterial.diffuse_color * hitMaterial.kd) + (hitMaterial.specular_color * (hitMaterial.ks * 10 * pow(cos_h, 10) / (2 * M_PI)));

	directLight = li * brdf * cos_nl * cos_nln / (2 * M_PI * pow(gk::Distance(hitPoint, lightPoint), 2) * 0.0025f);
	directLight /= lightPointPdf;
      }

      directLightSum += directLight;
    }

    // Indirect lighting
    

    return gk::Vec4(directLightSum / _directLightingSampleCount);
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

  gk::Vec4 energy;

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

      energy = incidentLight(worldOrigin, gk::Normalize(worldRay), worldRay.Length());
      energy.a = 1;

      outputImage->setPixel(x, y, energy);
    }
  }

  gk::ImageIO::writeImage(_outputImageFilename, outputImage);

  delete outputImage;
  deleteScene();

  return 0;
}
