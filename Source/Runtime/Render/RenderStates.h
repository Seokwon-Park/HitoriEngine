#pragma once

enum class ERasterizerState : uint8 { SolidBack, SolidNone, SolidFront, Wireframe, ShadowDepth, Count };
enum class EDepthStencilState : uint8 { Default, ReadOnly, Disabled, StencilMask, StencilOutline, PrepassReadOnly, Count };
enum class EBlendState : uint8 { Opaque, AlphaBlend, NoColorWrite, Additive, ShadowMaskR, ShadowMaskG, ShadowMaskB, ShadowMaskA, Count };
enum class ESamplerState : uint8 { LinearClamp, LinearWrap, ShadowCompare, Count };
