#type vertex
#version 450 core
		
layout(location = 0) in vec3 a_Position;		
layout(location = 1) in vec4 a_Color;		
layout(location = 2) in vec2 a_TexCoord;	
layout(location = 3) in float a_TexID;	
layout(location = 4) in float a_TilingFactor;	
layout(location = 5) in int a_EntityID;	


layout(std140, set = 0, binding = 0) uniform Camera
{
	vec4 View;
	mat4 Projection;
	mat4 ViewProjection;
} u_Camera;

/*
layout(set = 1, binding = 0) uniform Object
{
	mat4 ModelMatrix;
}u_Object;*/



struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
	float TexID;
	float TilingFactor;
};

layout(location = 0) out VertexOutput Output;
layout(location = 4) out flat int o_EntityID;


void main()
{
	Output.Color = a_Color;
	Output.TexCoord = a_TexCoord;
	Output.TexID = a_TexID;
	Output.TilingFactor = a_TilingFactor;
	o_EntityID = a_EntityID;

	//const array of positions for the triangle
	const vec3 positions[3] = vec3[3](
		vec3(1.f,1.f, 0.0f),
		vec3(-1.f,1.f, 0.0f),
		vec3(0.f,-1.f, 0.0f)
	);

	//output the position of each vertex
	gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
	//gl_Position = u_Camera.ViewProjection * vec4(a_Position, 1.0f);
}

#type fragment
#version 450 core
		
layout(location = 0) out vec4 color;
layout(location = 1) out int entityID;

struct VertexInput
{
	vec4 Color;
	vec2 TexCoord;
	float TexID;
	float TilingFactor;
};

layout(location = 0) in VertexInput Input;
//layout(location = 4) in flat int v_EntityID;

//layout(binding = 0) uniform sampler2D u_Textures[32];
//layout(set = 2, binding = 0) uniform sampler2D u_Texture;

void main()
{
	vec4 texColor = Input.Color;

	/*switch(int(Input.TexID))
	{
		case 0: texColor *= texture(u_Textures[0], Input.TexCoord * Input.TilingFactor); break;
		case 1: texColor *= texture(u_Textures[1], Input.TexCoord * Input.TilingFactor); break;
		case 2: texColor *= texture(u_Textures[2], Input.TexCoord * Input.TilingFactor); break;
		case 3: texColor *= texture(u_Textures[3], Input.TexCoord * Input.TilingFactor); break;
		case 4: texColor *= texture(u_Textures[4], Input.TexCoord * Input.TilingFactor); break;
		case 5: texColor *= texture(u_Textures[5], Input.TexCoord * Input.TilingFactor); break;
		case 6: texColor *= texture(u_Textures[6], Input.TexCoord * Input.TilingFactor); break;
		case 7: texColor *= texture(u_Textures[7], Input.TexCoord * Input.TilingFactor); break;
		case 8: texColor *= texture(u_Textures[8], Input.TexCoord * Input.TilingFactor); break;
		case 9: texColor *= texture(u_Textures[9], Input.TexCoord * Input.TilingFactor); break;
		case 10: texColor *= texture(u_Textures[10], Input.TexCoord * Input.TilingFactor); break;
		case 11: texColor *= texture(u_Textures[11], Input.TexCoord * Input.TilingFactor); break;
		case 12: texColor *= texture(u_Textures[12], Input.TexCoord * Input.TilingFactor); break;
		case 13: texColor *= texture(u_Textures[13], Input.TexCoord * Input.TilingFactor); break;
		case 14: texColor *= texture(u_Textures[14], Input.TexCoord * Input.TilingFactor); break;
		case 15: texColor *= texture(u_Textures[15], Input.TexCoord * Input.TilingFactor); break;
		case 16: texColor *= texture(u_Textures[16], Input.TexCoord * Input.TilingFactor); break;
		case 17: texColor *= texture(u_Textures[17], Input.TexCoord * Input.TilingFactor); break;
		case 18: texColor *= texture(u_Textures[18], Input.TexCoord * Input.TilingFactor); break;
		case 19: texColor *= texture(u_Textures[19], Input.TexCoord * Input.TilingFactor); break;
		case 20: texColor *= texture(u_Textures[20], Input.TexCoord * Input.TilingFactor); break;
		case 21: texColor *= texture(u_Textures[21], Input.TexCoord * Input.TilingFactor); break;
		case 22: texColor *= texture(u_Textures[22], Input.TexCoord * Input.TilingFactor); break;
		case 23: texColor *= texture(u_Textures[23], Input.TexCoord * Input.TilingFactor); break;
		case 24: texColor *= texture(u_Textures[24], Input.TexCoord * Input.TilingFactor); break;
		case 25: texColor *= texture(u_Textures[25], Input.TexCoord * Input.TilingFactor); break;
		case 26: texColor *= texture(u_Textures[26], Input.TexCoord * Input.TilingFactor); break;
		case 27: texColor *= texture(u_Textures[27], Input.TexCoord * Input.TilingFactor); break;
		case 28: texColor *= texture(u_Textures[28], Input.TexCoord * Input.TilingFactor); break;
		case 29: texColor *= texture(u_Textures[29], Input.TexCoord * Input.TilingFactor); break;
		case 30: texColor *= texture(u_Textures[30], Input.TexCoord * Input.TilingFactor); break;
		case 31: texColor *= texture(u_Textures[31], Input.TexCoord * Input.TilingFactor); break;
	}*/
	//texColor *= texture(u_Texture, Input.TexCoord * Input.TilingFactor);
	color = vec4(1.0, 0.0, 1.0, 1.0);
	//entityID = v_EntityID;
}
