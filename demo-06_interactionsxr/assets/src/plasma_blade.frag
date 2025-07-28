// Copyright 2024-25 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450
#define MAX_POINT_LIGHTS 3
#define MAX_SPOTLIGHTS 2

#define NOISE_SCALE_X  12.9898    // UV noise scaling factor for X
#define NOISE_SCALE_Y  78.233     // UV noise scaling factor for Y
#define NOISE_SPREAD   43758.5453 // Noise distribution multiplier
#define NOISE_INTENSITY 0.5       // How strong the noise pattern is
#define NOISE_BASE     1.0        // Base level for the noise

#define GLOW_OUTER     0.8        // Outer edge of the glow
#define GLOW_INNER     0.1        // Inner edge of the glow
#define BRIGHTNESS     3.0        // Overall brightness multiplier
#define COLOR_SPEED    2.0        // Speed of color animation

// Material UBO
layout(set = 0, binding = 0) uniform MaterialUBO {
    vec4 baseColorFactor;
    vec4 emissiveFactor;    // emissiveFactor.w used for receiving time
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float normalScale;
    uint textureFlags;
    int alphaMode;
    vec2 padding;     
} material;

// Scene Lighting UBO
    
// Main directional light
struct DirectionalLight {
    vec3 direction;
    float intensity;
    vec3 color;
};

// Point lights
struct PointLight {
    vec3 position;
    float range;
    vec3 color;
    float intensity;
};

// Spot lights
struct SpotLight {
    vec3 position;
    float range;
    vec3 direction;
    float intensity;
    vec3 color;
    float innerCone;
    float outerCone;
};

// Tonemapping parameters with packed flags
struct Tonemapping {
    float exposure;
    float gamma;
    uint tonemap;      // Bits 0-3: tonemap operator, Bits 4-5: render mode, Bits 6-31: available
    float contrast;
    float saturation;
};

layout(set = 1, binding = 0) uniform SceneLighting {
    // Scene lighting
    DirectionalLight mainLight;
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOTLIGHTS];
    // Scene ambient and active lights
    vec3 ambientColor;
    float ambientIntensity;
    uint activePointLights;
    uint activeSpotLights;
    Tonemapping tonemapping;
} scene;

// Texture samplers
layout(binding = 1) uniform sampler2D baseColorMap;
layout(binding = 2) uniform sampler2D metallicRoughnessMap;
layout(binding = 3) uniform sampler2D normalMap;
layout(binding = 4) uniform sampler2D emissiveMap;
layout(binding = 5) uniform sampler2D occlusionMap;

// Input from vertex shader
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

// Output
layout(location = 0) out vec4 outColor;

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Improved noise function for more dynamic effect
float plasma_noise(vec2 uv, float time) {
    // Create a moving wave pattern
    float wave = sin(uv.y * 10.0 + time * 3.0) * 0.5 + 0.5;
    
    // Add some horizontal variation
    wave += sin(uv.x * 5.0 + time * 2.0) * 0.25;
    
    // Add noise pattern
    float noise = fract(sin(dot(uv, vec2(NOISE_SCALE_X, NOISE_SCALE_Y))) * NOISE_SPREAD);
    
    return mix(wave, noise, 0.5) * NOISE_INTENSITY + NOISE_BASE;
}

void main() {
    
    float time = material.emissiveFactor.w * COLOR_SPEED;
    
    // Create base hue that changes over time and varies along the blade
    float baseHue = fract(time * 0.1 - inUV.y * 0.5); 
    
    // Add a secondary hue variation
    float hueVariation = sin(time * 2.0 - inUV.y * 6.0) * 0.1;
    float finalHue = fract(baseHue + hueVariation);
    
    // Convert to RGB
    vec3 color = hsv2rgb(vec3(finalHue, 1.0, 1.0));
    
    // Calculate distance from center
    vec2 center = vec2(0.5, 0.5);
    float dist = length(inUV - center);
    
    // Dynamic pattern
    float pattern = plasma_noise(inUV, time);
    
    // Glow effect
    float glow = smoothstep(GLOW_OUTER, GLOW_INNER, dist);
    
    // Combine everything
    color = color * glow * pattern * BRIGHTNESS;
    
    // Add subtle pulsing to the brightness
    float pulse = 1.0 + sin(time * 3.0) * 0.1;
    color *= pulse;
    
    outColor = vec4(color, 1.0);
}