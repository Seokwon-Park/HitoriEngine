#pragma once

enum class EShaderType
{
	Vertex,
	Pixel,
	Compute,
};

enum class ERenderingPath
{
    Forward,
    Deferred
};

enum class ELightingModel
{
    Gouraud,
    Lambert,
    Phong
};

enum class ELightCullingMode
{
    Off = 0,
    Tiled = 1,
    Clustered = 2,
    Tiled25D = 3
};