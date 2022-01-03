# HLSL Material for Unreal Engine

Ever wanted to write complex material functions directly in HLSL? Now you can!

Unreal Engine 4.26, 4.27 and 5.0 are supported. 4.25 cannot be supported as custom nodes have a single output in that version.

Demo: https://twitter.com/phyronnaz/status/1452553917204733953

Discord: https://discord.gg/UjtexdkWxC

Marketplace: https://www.unrealengine.com/marketplace/en-US/product/hlsl-material

## Features
* HLSL support: write all your functions in a single hlsl file and use any of them in regular materials
* Team-friendly: regular material functions are generated, so your team members don't need the plugin to use them!
* Live updates: material functions & opened material editors are refreshed when saving the hlsl file (Windows only)
* Comment support: comments are parsed & pin tooltips are set accordingly
* Smart updates: only modified functions are updated
* Texture parameters support
* Bool parameters support
* Define support
* Includes support (with auto updates when included files are edited)
* Default value support
* Clickable errors: errors are properly displayed relative to your file(s), and clicking them will open your IDE

## Installing from source
Download the repo as a zip and extract it under your project Plugins folder so you have `YourProject/Plugins/HLSLMaterial/HLSLMaterial.uplugin`

Visual Studio will be required.

## How to
* Create a new `HLSL Material Function Library` (right click Content Browser -> Material & Textures). This asset will be the link between your hlsl file and all the generated material functions.
* Set the `File` on it to point to your HLSL file
* Add functions to the file
* Material functions will be created when you save the HLSL file
* You can also disable the automatic updates and manually right click the asset -> `Update from HLSL`

## Syntax
* All return types must be `void` to ensure the pins are all properly named
* To mark a parameter as an output, use `out`: eg, `out float3 MyOutput`
* Comments must use the `//` syntax, `/*` is not supported
* `@param` in comments will be parsed & put into the pin tooltips

```hlsl
// Ray-sphere intersection
// @param   RayOrigin       The origin of the ray
// @param   RayDirection    The direction of the ray
// @param   SphereCenter    The center of the sphere
// @param   SphereRadius    The radius of the sphere
// @param   Distance        The distance from the ray origin to the hit on the sphere
void RaySphereIntersect(float3 RayOrigin, float3 RayDirection, float3 SphereCenter, float SphereRadius, out float Distance) 
{
    float a = dot(RayDirection, RayDirection);
    float3 SphereCenterToRayOrigin = RayOrigin - SphereCenter;
    float b = 2.0 * dot(RayDirection, SphereCenterToRayOrigin);
    float c = dot(SphereCenterToRayOrigin, SphereCenterToRayOrigin) - (SphereRadius * SphereRadius);
    float Discriminant = b * b - 4.0 * a* c;

    if (Discriminant < 0.0) 
    {
        Distance = -1.0;
    }
    else
    {
      Distance = (-b - sqrt(Discriminant)) / 2 * a;
    }
}

#define BLUE 1

void Test(
    out float3 Color,
    float Red = 1.f,
    bool Green = false,
    Texture2D Texture,
    SamplerState TextureSampler,
    FMaterialPixelParameters Parameters)
{
    Color.r = Red;
    Color.g = Green ? 1 : 0;
    Color.b = BLUE;

    const float2 UVs = GetSceneTextureUV(Parameters);
    Color = Texture2DSample(Texture, TextureSampler, UVs);
}
```

### Texture
For textures, an additional parameter named `YourTextureParameterSampler` of type `SamplerState` will automatically be added by Unreal:

```hlsl
void Test(Texture2D MyTexture, float2 UVs, out float3 Color)
{
	Color = Texture2DSample(MyTexture, MyTextureSampler, UVs).rgb;
}
```

## How it works

The plugin manually parses the functions in the HLSL file. From there, it creates new material functions with a Custom node holding the function body.

The logic is pretty simple & straightforward, so it should be relatively robust.

## Project management
### Moving functions
Material functions are generated next to your library asset, under `YourFunctionLibraryAsset_Generated/`.
Once they are generated, you should be able to move them anywhere - the library keeps a ref to them.

### Renaming functions
If you rename a function in your HLSL file, you need to manually rename the corresponding material function in Unreal, ideally before saving the HLSL. Otherwise, a new one will be created with the new name.

### Deleting functions
The plugin will never delete any asset. If you remove a function from the HLSL, it is up to you to remove it from Unreal.

### Source Control
You don't need to check in the HLSL file or the library - simply checking in the generated functions should be enough. However, if your teammates are also using the plugin it might be best to check in everything.
