#define _USE_MATH_DEFINES
#include <math.h>
#include "marMatrix2.h"

float getrad(float degree)
{
  return 2.0 * M_PI * degree / 360.0;
}

float getdeg(float theta)
{
  return 360.0 * theta / (2.0 * M_PI);
}

vvec3 cross_product(vvec3 v1, vvec3 v2)
{
  vvec3 ret;
  ret.x = v1.y * v2.z - v1.z * v2.y;
  ret.y = v1.z * v2.x - v1.x * v2.z;
  ret.z = v1.x * v2.y - v1.y * v2.x;
  return ret;
}

void matget_rotate(const float theta, const bool is_axis_x, const bool is_axis_y, const bool is_axis_z, smat4& mat, const bool transpose)
{
  float sin_theta = sin(theta);
  float cos_theta = cos(theta);

  for(int i = 0; i < 4; i++)
    for(int j = 0; j < 4; j++)
      mat.m[i][j] = (i == j) ? 1.0f : 0.0f;

  if (is_axis_x) {
    mat.m[1][1] = cos_theta;
    mat.m[1][2] = transpose ? sin_theta : -sin_theta;
    mat.m[2][1] = transpose ? -sin_theta : sin_theta;
    mat.m[2][2] = cos_theta;
  } else if (is_axis_y) {
    mat.m[0][0] = cos_theta;
    mat.m[0][2] = transpose ? -sin_theta : sin_theta;
    mat.m[2][0] = transpose ? sin_theta : -sin_theta;
    mat.m[2][2] = cos_theta;
  } else if (is_axis_z) {
    mat.m[0][0] = cos_theta;
    mat.m[0][1] = transpose ? sin_theta : -sin_theta;
    mat.m[1][0] = transpose ? -sin_theta : sin_theta;
    mat.m[1][1] = cos_theta;
  }
}

void matget_lookat(const vvec3 &cameraPos, const vvec3 &lookAtPos, const vvec3 &up, smat4 &mat, const bool transpose) {
  smat4 m_cam, m_offset;
  m_cam.clear();

  // カメラ前方へのベクトル
  vvec3 f = lookAtPos - cameraPos;
  f.normalize_self();

  // カメラ横方向へのベクトル
  vvec3 s = cross_product(f, up);
  s.normalize_self();

  // アップベクトル
  // 引数のアップベクトルは、アップベクトルとして期待しているものの、それほど信用してはいない
  vvec3 u = cross_product(s, f);

  m_cam.m[0][0] = s.x;
  m_cam.m[1][0] = s.y;
  m_cam.m[2][0] = s.z;
  m_cam.m[0][1] = u.x;
  m_cam.m[1][1] = u.y;
  m_cam.m[2][1] = u.z;
  m_cam.m[0][2] = -f.x;
  m_cam.m[1][2] = -f.y;
  m_cam.m[2][2] = -f.z;
  m_cam.m[3][3] = 1.0f;

  m_offset.clear();
  m_offset.m[0][0] = 1.0f;
  m_offset.m[1][1] = 1.0f;
  m_offset.m[2][2] = 1.0f;
  m_offset.m[3][3] = 1.0f;
  m_offset.m[3][0] = -cameraPos.x;
  m_offset.m[3][1] = -cameraPos.y;
  m_offset.m[3][2] = -cameraPos.z;

  mat = m_offset * m_cam;
  if (! transpose) {
    mat = mat.transpose();
  }
}

void matget_perspective(float fovY, float aspect, float z_near, float z_far, smat4 &m, bool transpose)
{
  float rad_fovy = (fovY / 360.0f) * 2 * M_PI;
  float f = 1.0f / tan(rad_fovy / 2.0f);

  m.clear();

  if (transpose) {
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = (z_far + z_near) / (z_near - z_far);
    m.m[2][3] = -1.0f;
    m.m[3][2] = 2.0f * z_far * z_near / (z_near - z_far);
  } else {
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = (z_far + z_near) / (z_near - z_far);
    m.m[3][2] = -1.0f;
    m.m[2][3] = 2.0f * z_far * z_near / (z_near - z_far);
  }
}
