#include "marQuatCamera.h"

// 方位角と仰角、そして距離を与えることでカメラ初期位置を決定する
TQuatCamera::TQuatCamera(float theta, float phi, float distance)
     : distance(distance)
{
  // 仰角がプラス（0～90度）のとき、カメラはx軸に対してマイナス方向に回転する
  phi = -phi;

  // カメラ初期位置の設定は基本の軸に対して行う（= 軸に対する補正なし）

  // 先に仰角用の軸を与える
  vvec3 axis_x = {1, 0, 0};
  qquat q_phi(phi, axis_x);

  accumQuat = q_phi;
  update();

  // 次に方位角用の軸を与える
  vvec3 axis_y = {0, 1, 0};
  qquat q_theta(theta, axis_y);

  // 累積のクォータニオンを計算
  accumQuat = q_theta * accumQuat;

  near_z = distance * NEAR_COEF;
  far_z = distance + FAR_COEF;

  update();
}

void TQuatCamera::update()
{
  vvec3 up = {0, 1, 0};
  up_vector = accumQuat * up;
  position = accumQuat * vvec3(0, 0, distance);
}

qquat TQuatCamera::getDragstartQuat()
{
  return dragstartQuat;
}

void TQuatCamera::setAspectRatio(int client_width, int client_height)
{
  if (client_height == 0) {
    client_height = 1; // Zero除算を防ぐために高さを1に設定
  }
  aspectRatio = static_cast<float>(client_width) / static_cast<float>(client_height);
}

void TQuatCamera::dragRotate(qquat q_delta)
{
  accumQuat = q_delta * dragstartQuat;

  update();
}

void TQuatCamera::OnMouseDown(int mouseX, int mouseY)
{
  dragstartQuat = accumQuat;
  dragstartDistance = distance;
}

void TQuatCamera::setDistance(float delta)
{
  // デルタを反映させた距離を求める
  float tmp_distance = dragstartDistance + delta;

  // 求めた距離が近接限界を超えていないかをチェックする
  // distance - near_z > zoomLimitDistance であること
  if (tmp_distance < zoomLimitDistance + near_z) {
    tmp_distance = zoomLimitDistance + near_z;
  }
  distance = tmp_distance;

  update();
}

// 近接限界を設定する
void TQuatCamera::SetZoomLimitDistance(double distance) {
  zoomLimitDistance = distance;
}
