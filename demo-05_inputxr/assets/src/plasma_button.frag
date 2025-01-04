// Copyright 2024-25 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450
#define MAX_POINT_LIGHTS 3
#define MAX_SPOTLIGHTS 2

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

void main() {
    
    float distanceFromCenter = length(inUV - vec2(0.5));

    // Create radial gradient 
    float gradient = smoothstep(0.0, 1.0, 1.0 - distanceFromCenter);
    vec3 gradientColor = material.baseColorFactor.xyz * gradient;

    // Output color
    outColor = vec4(gradientColor, 0.35);
}