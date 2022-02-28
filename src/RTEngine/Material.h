#ifndef MATERIAL_H
#define MATERIAL_H



// Material is material from engine perspective, maps to constant buffer (hlsl perspective)
struct Material
{
    float reflectanceCoef = 0.0f;
    float diffuseCoef = 0.9f;
    float specularCoef = 0.7f;
    float specularPower = 50.0f;
    float stepScale = 1.0f;
    float refractionIndex = 0.0f;
    float fuzz = 1.0f;
    bool hasTexture = false;
    bool hasPerlin = false;
};

#endif // !MATERIAL_H
