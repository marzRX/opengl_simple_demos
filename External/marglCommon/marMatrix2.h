#pragma once

#include <math.h>

float getrad(float degree);
float getdeg(float theta);

//----------------------------------------------------------------------
// ベクトル
//----------------------------------------------------------------------
struct vvec3
{
  float x, y, z;

  vvec3(float x = 0.0f, float y = 0.0f, float z = 0.0f)
       : x(x), y(y), z(z) {}

  vvec3 operator - (const vvec3 &v) const {
    return vvec3{x-v.x, y-v.y, z-v.z};
  }

  float length() const {
    return sqrt(x * x + y * y + z * z);
  }

  // 長さを 1 にする
  void normalize_self() {
    float len = length();
    if (len > 0) { // 0で割ることを防ぐ
      x /= len;
      y /= len;
      z /= len;
    }
  }
};

vvec3 cross_product(vvec3 v1, vvec3 v2);

//----------------------------------------------------------------------
// クォータニオン(回転)
//----------------------------------------------------------------------
struct qquat {
  float w, x, y, z;

  static constexpr float EPSILON = 1e-12;

  // 回転なし
  qquat() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}

  qquat(float w, float x, float y, float z)
       : w(w), x(x), y(y), z(z) {}

  // 軸 axis まわりに theta(ラジアン)回転するクォータニオン
  qquat(float theta, const vvec3& axis) {
    // 軸ベクトルを正規化
    float len = axis.length();
    if (len <= EPSILON) {
      // 軸の大きさがゼロのときはノーマライズできないので、その際は0度で回転
      len = 1.0;
      theta = 0.0;
    }
    float axis_nx = axis.x / len;
    float axis_ny = axis.y / len;
    float axis_nz = axis.z / len;

    float half_theta = theta * 0.5f;
    float s = sin(half_theta);

    w = cos(half_theta);
    x = axis_nx * s;
    y = axis_ny * s;
    z = axis_nz * s;
  }

  // クォータニオン同士の掛け算（新しい回転 x 回転の累積）
  // A * B の場合、A は *this、B は引数 q となる。
  qquat operator*(const qquat& q) const {
    return qquat(
      w * q.w - x * q.x - y * q.y - z * q.z,
      w * q.x + x * q.w + y * q.z - z * q.y,
      w * q.y - x * q.z + y * q.w + z * q.x,
      w * q.z + x * q.y - y * q.x + z * q.w
      );
  }

  // ベクトルをクォータニオンで回転させる
  vvec3 operator*(const vvec3& v) const {
    // ペアとなるクォータニオンを作る
    qquat qR = {w, -x, -y, -z};
    // 頂点 v をクォータニオンで表現
    qquat qP = {0.0, v.x, v.y, v.z};
    qquat q_pos = qquat(w, x, y, z) * qP * qR;
    return vvec3(q_pos.x, q_pos.y, q_pos.z);
  }
};

//----------------------------------------------------------------------
// 行列
//----------------------------------------------------------------------
// 3x3 (法線行列用)
struct smat3 {
  float m[3][3];

  smat3() {
    for(int i = 0; i < 3; i++)
      for(int j = 0; j < 3; j++)
        m[i][j] = 0.0f;
  }
};

// 4x4
struct smat4
{
  float m[4][4];

  // 全要素 0
  smat4() {
    clear();
  }

  void clear() {
    for(int i = 0; i < 4; i++)
      for(int j = 0; j < 4; j++)
        m[i][j] = 0.0f;
  }

  smat4 transpose() const {
    smat4 result;
    for(int i = 0; i < 4; i++)
      for(int j = 0; j < 4; j++)
        result.m[j][i] = m[i][j];
    return result;
  }

  smat4 operator * (const smat4 &rhs) const {
    smat4 result;
    for(int i = 0; i < 4; ++i)
      for(int j = 0; j < 4; ++j)
        for(int k = 0; k < 4; ++k)
          result.m[i][j] += m[i][k] * rhs.m[k][j];
    return result;
  }
};

//----------------------------------------------------------------------
// 行列を作る関数
//----------------------------------------------------------------------
// 回転行列。theta はラジアン。回転軸は is_axis_x/y/z のどれか1つを true にする
void matget_rotate(const float theta, const bool is_axis_x, const bool is_axis_y, const bool is_axis_z, smat4& mat, const bool transpose=true);

// ビュー行列
void matget_lookat(const vvec3 &cameraPos, const vvec3 &lookAtPos, const vvec3 &up, smat4 &mat, const bool transpose=true);

// 透視投影行列。fovY は度
void matget_perspective(float fovY, float aspect, float z_near, float z_far, smat4 &m, bool transpose=true);
