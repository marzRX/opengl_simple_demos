#pragma once

#include "marMatrix2.h"

const float CAMERA_DISTANCE = 10.0;
const float FOV_DEFAULT = 40.0;
const float ASPECT_RATIO_DEFAULT = 1.0;
const float NEAR_COEF = 0.1;
const float FAR_COEF = 1000.0;

class TQuatCamera {
private:
  // 累積のクォータニオン
  qquat accumQuat = {1, 0, 0, 0};

  // マウスを押した時点の、回転と距離
  qquat dragstartQuat = {1, 0, 0, 0};
  float dragstartDistance = CAMERA_DISTANCE;

  // 近接限界: distance - near_z > zoomLimitDistance であること
  float zoomLimitDistance = 0.0;

public:
  // 方位角と仰角(ラジアン)、そして距離を与えることでカメラ初期位置を決定する
  TQuatCamera(float theta, float phi, float distance);

  // マウスドラッグ時の回転（マウスダウン時の回転状態を起点とした回転）
  void dragRotate(qquat q_delta);
  qquat getDragstartQuat();

  void setAspectRatio(int client_width, int client_height);

  // マウスダウン時の距離にデルタを反映させた距離にする
  void setDistance(float delta);
  void SetZoomLimitDistance(double distance);

  void OnMouseDown(int mouseX, int mouseY);

  // position と up_vector を、累積の回転から求める
  void update();

  vvec3 position = {0, 0, CAMERA_DISTANCE}; // カメラ位置(view)
  vvec3 target = {0, 0, 0}; // 注視点(center)
  vvec3 up_vector = {0, 1, 0}; // アップベクトル(up)

  float distance = CAMERA_DISTANCE;
  float near_z;
  float far_z;

  float fov = FOV_DEFAULT;
  float aspectRatio = ASPECT_RATIO_DEFAULT;
};
