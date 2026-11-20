#version 450

in vec3 vColor;
in vec2 vUV;
in float vLight;

out vec4 FragColor;

layout(binding = 0) uniform sampler2D image;

// 0.0: 頂点カラー(従来の表示)  1.0: テクスチャ x 陰影
uniform float uUseTexture;

void main()
{
  vec3 tex = texture(image, vUV).rgb;
  vec3 col = mix(vColor, vec3(vLight) * tex, uUseTexture);

  FragColor = vec4(col, 1.0);
}
