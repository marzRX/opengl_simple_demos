#version 430

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aUV;
layout (location = 0) out vec3 vColor;
layout (location = 1) out vec2 vUV;

layout (location = 0) uniform mat4 uMVPMatrix;
layout (location = 1) uniform mat4 uModelViewMatrix;
layout (location = 2) uniform mat4 uViewMatrix;
layout (location = 3) uniform mat3 uNormalMatrix;

layout (location = 4) uniform vec3 uDiffuseColor;
layout (location = 5) uniform vec3 uAmbientColor;

void main()
{
  vUV = aUV;

  vec4 lightPos = vec4(5.0, 10.0, 50.0, 1.0);

//  vec3 diffuse_color = vec3(0.64, 0.64, 0.64);
  vec3 diffuse_color = uDiffuseColor;
//  vec3 ambient_color = vec3(0.2, 0.2, 0.2);
  vec3 ambient_color = uAmbientColor;

  vec4 vertex = vec4(aPosition, 1.0);
  vec3 position = vec3(uModelViewMatrix * vertex);
  vec3 normal = uNormalMatrix * aNormal;

  vec3 lightlight = vec3(uViewMatrix * lightPos);
  vec3 lightVec = lightlight - position;

  float dis = length(lightVec);
  lightVec = normalize(lightVec);

  float diffuse = dot(lightVec, normal);

//  vColor = ambient_color;
  vColor = ambient_color * diffuse_color * 0.01;
//  vColor = vec3(aColor * 0.01 + diffuse_color * 0.99);


  if (diffuse > 0.0) {
    vColor += vec3(aColor * diffuse);
  }


  gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
}
