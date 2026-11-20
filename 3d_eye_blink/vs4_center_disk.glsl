#version 330

in vec3 aPosition;
in vec3 aColor;
in vec3 aNormal;
in vec2 aUV;
out vec3 vColor;
out vec2 vUV;
out float vLight; // 陰影の明るさだけ(テクスチャ表示のときに使う)

uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;

//uniform mat4 uMMRot;
uniform vec3 uModelCenter;
uniform float uTime;
uniform float uInstanceCount;
uniform float uRingBlocks;

const float M_PI = 3.1415926535;

mat4 rotation1(float degree) {
  
  float theta = 2.0 * M_PI * degree / 360.0;
  float cos_theta = cos(theta);
  float sin_theta = sin(theta);
  return mat4(cos_theta, sin_theta, 0.0, 0.0,
	      -sin_theta, cos_theta, 0.0, 0.0,
	      0.0, 0.0, 1.0, 0.0,
	      0.0, 0.0, 0.0, 1.0);
}

mat4 transformQuat2Mat(vec4 q_rot) {
  float qx = q_rot.x;
  float qy = q_rot.y;
  float qz = q_rot.z;
  float qw = q_rot.w;
  float m11 = 1.0 - 2.0 * qy * qy - 2.0 * qz * qz;
  float m12 = 2.0 * qx * qy + 2.0 * qw * qz;
  float m13 = 2.0 * qx * qz - 2.0 * qw * qy;
  float m21 = 2.0 * qx * qy - 2.0 * qw * qz;
  float m22 = 1.0 - 2.0 * qx * qx - 2.0 * qz * qz;
  float m23 = 2.0 * qy * qz + 2.0 * qw * qx;
  float m31 = 2.0 * qx * qz + 2.0 * qw * qy;
  float m32 = 2.0 * qy * qz - 2.0 * qw * qx;
  float m33 = 1.0 - 2.0 * qx * qx - 2.0 * qy * qy;

  mat4 m_rot = mat4(m11, m12, m13, 0.0,
		    m21, m22, m23, 0.0,
		    m31, m32, m33, 0.0,
		    0.0, 0.0, 0.0, 1.0);
  return m_rot;
}

vec4 quatget_qrot(vec4 q_stat) {
  float theta;
  float sinHalfTheta;
  vec3 axis;
  float axis_len = sqrt(q_stat.x * q_stat.x +
			q_stat.y * q_stat.y +
			q_stat.z * q_stat.z);
  if (axis_len <= 1e-12) {
    axis_len = 1.0;
    q_stat.w = 0.0;
  }

  axis = vec3(q_stat) / axis_len;
  theta = q_stat.w / 180.0 * M_PI;
  sinHalfTheta = sin(theta / 2.0);

  vec4 q;
  q.xyz = axis * sinHalfTheta;
  q.w = cos(theta / 2.0);
  return q;
}

void main()
{
  vec4 lightPos = vec4(5.0, 10.0, 50.0, 1.0);

//  vec3 diffuse_color = vec3(0.64, 0.64, 0.64);
  vec3 diffuse_color = vec3(1.0);
  vec3 ambient_color = vec3(0.3, 0.3, 0.1);

  vec4 vertex = vec4(aPosition, 1.0);

  float deg_core = 360.0 / uRingBlocks;
  float degree = deg_core * float(gl_InstanceID);
  vec4 q_stat = vec4(0.0, 1.0, 0.0, degree);
  vec4 q_rot = quatget_qrot(q_stat);
  mat4 m_rot = transformQuat2Mat(q_rot);

  // まぶたのため
  float degree2 = -90.0 * uTime;
  vec4 q_stat2 = vec4(1.0, 0.0, 0.0, degree2);
  vec4 q_rot2 = quatget_qrot(q_stat2);
  mat4 m_rot2 = transformQuat2Mat(q_rot2);

  mat4 m_i = mat4(1.0, 0.0, 0.0, 0.0,
		  0.0, 1.0, 0.0, 0.0,
		  0.0, 0.0, 1.0, 0.0,
		  0.0, 0.0, 0.0, 1.0);


//  mat4 ModelMatrix = m_rot;// * m_rot2;
  mat4 ModelMatrix = m_rot2 * m_rot;
  mat4 ModelViewMatrix = uViewMatrix * ModelMatrix;
  mat3 NormalMatrix = mat3(ModelViewMatrix);
  mat4 MVPMatrix = uProjMatrix * ModelViewMatrix;

  vec3 position = vec3(ModelViewMatrix * vertex);
  vec3 normal = vec3(NormalMatrix * aNormal);

  vec3 lightlight = vec3(uViewMatrix * lightPos);
  vec3 lightVec = lightlight - position;

  lightVec = normalize(lightVec);

  float diffuse = dot(lightVec, normal);

  vColor = ambient_color;
  if (diffuse > 0.0) {
    vColor = vec3(aColor * diffuse);
  }

  vUV = aUV;
  vLight = (diffuse > 0.0) ? diffuse : 0.3;

  vec3 move = (uModelCenter) * uTime;
  vec3 aaPosition = aPosition + move;
  gl_Position = MVPMatrix * vec4(aaPosition, 1.0);
}
