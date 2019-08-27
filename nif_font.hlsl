#include "globals.hlsl"

Texture2D shaderTexture : register(t0);		//font texture
Texture2DArray texArray : register(t1);		//frame, icons, etc

cbuffer cb0 : register(b0)
{
	float4 colors[32];
};


struct VertexInputType
{
    float2 position : POSITION;			//input pos is in ndc space 
    float2 tex : TEXCOORD0;
	uint2 colorSlice : COLOR_SLICE;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
	float3 color : COLOR;
	uint slice: SLICE;
};

PixelInputType VS(VertexInputType input)
{
    PixelInputType output;
	output.position = float4(input.position, 0, 1);			//pre-transformed vertices
	output.tex = input.tex;
	output.color = colors[input.colorSlice.x].xyz;
	output.slice = input.colorSlice.y;
    return output;
}

float4 PS(PixelInputType input) : SV_TARGET
{
	float4 texColor;

	if(input.slice == 255){ 
		//sample from font texture
		texColor = shaderTexture.Sample(PointSampler, input.tex);
		// If the texColor is black on the texture then treat this pixel as transparent.
		if(texColor.r == 0.0f){
			texColor.a = 0.0f;
		}else{
			texColor.a = 1.0f;
		}
		
		texColor.xyz = input.color;
	}else{
		texColor = texArray.Sample(PointSampler, float3(input.tex, input.slice));
		texColor.xyz = texColor.xyz * input.color;
	}
	
    return texColor;
}
