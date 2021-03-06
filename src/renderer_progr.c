/*
 dEngine Source Code 
 Copyright (C) 2009 - Fabien Sanglard
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  r_progr_renderer.c
 *  dEngine
 *
 *  Created by fabien sanglard on 09/08/09.
 *
 */

#include "renderer_progr.h"
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#include "camera.h"
#include "filesystem.h"
#include "world.h"
#include "stats.h"
#include "config.h"
#include "obj.h"

matrix_t projectionMatrix;
matrix_t modelViewMatrix;
matrix_t modelViewProjectionMatrix;

matrix_t* matrixStackPointer;
matrix_t matrixStack[16];

typedef struct simple_shader_t {
	GLuint prog;
	GLuint vars[32];
	uchar props;
} shader_prog_t;


int lastId;
int lastBumpId;
int lastSpecId;

shader_prog_t shaders[8];
#define UNIFIED_LIGHT_SHADER 0
#define STRING_RENDER_SHADER 1
#define UNIFIED_LIGHT_SHADOW_SHADER 2
#define SHADOW_GENERATOR_SHADER 3

#define SHADER_MVT_MATRIX 0
#define SHADER_ATT_VERTEX 1
#define SHADER_ATT_NORMAL 2
#define SHADER_ATT_UV 3
#define SHADER_ATT_TANGENT 4
#define SHADER_TEXT_COLOR_SAMPLER 5
#define SHADER_TEXT_BUMP_SAMPLER 6
#define SHADER_UNI_LIGHT_POS 7
#define SHADER_UNI_LIGHT_COL_AMBIENT 8
#define SHADER_UNI_LIGHT_COL_DIFFUSE 9
#define SHADER_UNI_MV_MATRIX 10
#define SHADER_LIGHTPOV_MVT_MATRIX 11
#define SHADER_TEXT_SHADOWMAP_SAMPLER 12
#define SHADER_UNI_CAMERA_POS 13
#define SHADER_UNI_LIGHT_COL_SPECULAR 14
#define SHADER_UNI_MATERIAL_SHININESS 15

#define SHADER_TEXT_SPEC_SAMPLER 16
#define SHADER_UNI_MAT_COL_SPECULAR 17

#define NUM_UBERSHADERS 256
shader_prog_t* ubershaders[NUM_UBERSHADERS];
char* uberShaderVertex = 0;
char* uberShaderFragment = 0;
char ubserShaderDefines[8][64] = 
{
	"#define BUMP_MAPPING\n",
	"#define SPEC_MAPPING\n",
	"#define DIFF_MAPPING\n",
	"\n",
	"\n",
	"\n",
	"\n",
	"#define SHADO_MAPPING\n"
};

char ubserShaderUndefines[8][64] = 
{
	"#undef BUMP_MAPPING\n",
	"#undef SPEC_MAPPING\n",
	"#undef DIFF_MAPPING\n",
	"\n",
	"\n",
	"\n",
	"\n",
	"#undef SHADO_MAPPING\n"
};

uchar PROP_NULL = 0;

shader_prog_t* currentShader;

GLuint shadowFBOId;
GLuint shadowMapTextureId;

float shadowMapRation = 1.5f;

int SRC_UseShader(shader_prog_t* shader)
{
	//if (currentShader == shader)
	//	return 0;
		
	
	STATS_AddShaderSwitch();
	
//	printf("[SRC_UseShader] Shader = %u\n",shader->prog);
	glUseProgram(shader->prog); 
	currentShader = shader;
	
	lastId = -1;
	lastBumpId = -1;
	lastSpecId = -1;
	
	return 1;
}




void CreateFBOandShadowMap()
{
	GLenum status;
	GLuint   depthRenderbuffer; 
	
	
	glGenFramebuffers(1, &shadowFBOId);
	glBindFramebuffer(GL_FRAMEBUFFER,shadowFBOId);
	
	
	
	glGenTextures(1, &shadowMapTextureId);
	glBindTexture(GL_TEXTURE_2D, shadowMapTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, renderWidth*shadowMapRation, renderHeight*shadowMapRation, 0, GL_RGB565, GL_UNSIGNED_SHORT_5_6_5, NULL);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderWidth*shadowMapRation, renderHeight*shadowMapRation, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, shadowMapTextureId,0);
	glBindTexture(GL_TEXTURE_2D, -1);
	
	glGenRenderbuffers(1, &depthRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer); 
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, renderWidth*shadowMapRation, renderHeight*shadowMapRation); 
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer); 
	
	
	
	
	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	
	switch (status) {
		case GL_FRAMEBUFFER_COMPLETE:						printf("GL_FRAMEBUFFER_COMPLETE\n");break;
		case 0x8CDB:										printf("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT\n");break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:			printf("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:	printf("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n");break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:			printf("GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS\n");break;			
		case GL_FRAMEBUFFER_UNSUPPORTED:					printf("GL_FRAMEBUFFER_UNSUPPORTED\n");break;	
		default:											printf("Unknown issue (%x).\n",status);break;	
	}	

	
	
	//glBindFramebuffer(GL_FRAMEBUFFER,0);
}


GLuint LoadShader(const char *shaderSrcPath, GLenum type, uchar props) 
{ 
	GLuint shader; 
	GLint compiled; 
	GLchar* sources[9];
	uchar i;
	int j;
	
	//printf("Loading %s\n",shaderSrcPath);
	
	memset(sources, 0, 9 * sizeof(GLchar*));
	
	// Create the shader object 
	shader = glCreateShader(type); 
	

	
	if(shader == 0) 
	{
		printf("Failed to created GL shader for '%s'\n",shaderSrcPath);
		return 0; 
	}
	
	const filehandle_t* shaderFile;
	
	shaderFile = FS_OpenFile(shaderSrcPath,"r");
	
	if (!shaderFile)
	{
		printf("Could not load shader: %s\n",shaderSrcPath);
		return 0;
	}
	
	i= 1 ;
	j= 0 ;
	do {
		
		if ((i & props) == i)
			sources[j] = ubserShaderDefines[j];
		else 
			sources[j] = ubserShaderUndefines[j] ;
		

		
		i *= 2;
		j++ ;
	} while (j< 8) ;
	
	sources[8] = (GLchar*)shaderFile->ptrStart;
	
	//for(i=0;i<8;i++)
	//	printf("%s",sources[i]);
	
	// Load the shader source 
	glShaderSource(shader, 9, (const GLchar**)sources, NULL); 
	

	
	
	
	// Compile the shader 
	glCompileShader(shader); 
	// Check the compile status 
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	
	if(!compiled) 
	{ 
		GLint infoLen = 0; 
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen); 
		
		if(infoLen > 1) 
		{ 
			char* infoLog = malloc(sizeof(char) * infoLen); 
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog); 
			printf("Error processing '%s' compiling shader:\n%s\n",shaderSrcPath, infoLog); 
			free(infoLog); 
		} 
		glDeleteShader(shader); 
		return 0; 
	} 
	return shader; 
}

void LoadProgram(shader_prog_t* shaderProg,const char* vertexShaderPath, const char* fragmentShaderPath, uchar props)
{
	GLuint vertexShader; 
	GLuint fragmentShader; 
	
	GLint linked;
	

	
	//Load simple shader
	vertexShader = LoadShader(vertexShaderPath,GL_VERTEX_SHADER,props);
	fragmentShader = LoadShader(fragmentShaderPath,GL_FRAGMENT_SHADER,props);
	
	
	// Create the program object 
	shaderProg->prog = glCreateProgram(); 
	if(shaderProg->prog == 0) 
	{
		printf("Could not create GL program.");
		return ; 
	}
	
	glAttachShader(shaderProg->prog, vertexShader); 
	glAttachShader(shaderProg->prog, fragmentShader);
	
	
	// Link the program 
	glLinkProgram(shaderProg->prog); 
	
	// Check the link status 
	glGetProgramiv(shaderProg->prog, GL_LINK_STATUS, &linked); 
	if(!linked) 
	{ 
		GLint infoLen = 0; 
		glGetProgramiv(shaderProg->prog, GL_INFO_LOG_LENGTH, &infoLen); 
		
		if(infoLen > 1) 
		{ 
			char* infoLog = malloc(sizeof(char) * infoLen); 
			glGetProgramInfoLog(shaderProg->prog, infoLen, NULL, infoLog); 
			printf("Error linking program:\n%s\n", infoLog); 
			
			free(infoLog); 
		} 
		glDeleteProgram(shaderProg->prog); 
		return ; 
	} 
	
}


void SRC_BindUberShader(uchar props)
{
	uchar res_props;
	
	res_props = props & renderer.props;

	
	if (!ubershaders[res_props])
	{
		currentShader = calloc(1,sizeof(shader_prog_t));
		currentShader->props = res_props;
		ubershaders[res_props] = currentShader ;
		
		//MATLIB_printProp(res_props);
		LoadProgram(currentShader,"data/shaders/v_uber.glsl","data/shaders/f_uber.glsl",res_props);
		
		
		
		//Get all uniforms and attributes
		
		currentShader->vars[SHADER_MVT_MATRIX]				= glGetUniformLocation(currentShader->prog,"modelViewProjectionMatrix");
		currentShader->vars[SHADER_TEXT_COLOR_SAMPLER]		= glGetUniformLocation(currentShader->prog,"s_baseMap");
		currentShader->vars[SHADER_TEXT_BUMP_SAMPLER]		= glGetUniformLocation(currentShader->prog,"s_bumpMap");	
		currentShader->vars[SHADER_UNI_LIGHT_POS]			= glGetUniformLocation(currentShader->prog,"lightPosition");
		currentShader->vars[SHADER_UNI_LIGHT_COL_AMBIENT]	= glGetUniformLocation(currentShader->prog,"lightColorAmbient");
		currentShader->vars[SHADER_UNI_LIGHT_COL_DIFFUSE]	= glGetUniformLocation(currentShader->prog,"lightColorDiffuse");
		currentShader->vars[SHADER_ATT_VERTEX]				= glGetAttribLocation(currentShader->prog,"a_vertex");
		currentShader->vars[SHADER_ATT_NORMAL]				= glGetAttribLocation(currentShader->prog,"a_normal");
		currentShader->vars[SHADER_ATT_UV]					= glGetAttribLocation(currentShader->prog,"a_texcoord0");
		currentShader->vars[SHADER_ATT_TANGENT]				= glGetAttribLocation(currentShader->prog,"a_tangent");
		currentShader->vars[SHADER_LIGHTPOV_MVT_MATRIX]		= glGetUniformLocation(currentShader->prog,"lightPOVPVMMatrix");
		currentShader->vars[SHADER_TEXT_SHADOWMAP_SAMPLER]	= glGetUniformLocation(currentShader->prog,"s_shadowpMap");
		currentShader->vars[SHADER_UNI_CAMERA_POS]			= glGetUniformLocation(currentShader->prog,"cameraPosition");
		currentShader->vars[SHADER_UNI_LIGHT_COL_SPECULAR]	= glGetUniformLocation(currentShader->prog,"lightColorSpecular");
		currentShader->vars[SHADER_UNI_MATERIAL_SHININESS]	= glGetUniformLocation(currentShader->prog,"materialShininess");
		currentShader->vars[SHADER_TEXT_SPEC_SAMPLER]		= glGetUniformLocation(currentShader->prog,"s_specularMap");
		currentShader->vars[SHADER_UNI_MAT_COL_SPECULAR]	= glGetUniformLocation(currentShader->prog,"matColorSpecular");
		
	}
	
	if (SRC_UseShader(ubershaders[res_props]) )
	{
		if ((currentShader->props & PROP_SHADOW) == PROP_SHADOW)
		{
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D,shadowMapTextureId);
			glUniform1i ( currentShader->vars[SHADER_TEXT_SHADOWMAP_SAMPLER], 3 );
		}
	
		if ((currentShader->props & PROP_BUMP) == PROP_BUMP)
			glEnableVertexAttribArray(currentShader->vars[SHADER_ATT_TANGENT] );
		else
			glDisableVertexAttribArray(currentShader->vars[SHADER_ATT_TANGENT]);

		glEnableVertexAttribArray(currentShader->vars[SHADER_ATT_UV]);
		glEnableVertexAttribArray(currentShader->vars[SHADER_ATT_VERTEX] );
		glEnableVertexAttribArray(currentShader->vars[SHADER_ATT_NORMAL] );
	

	
		//Setup light
		glUniform3fv(currentShader->vars[SHADER_UNI_LIGHT_COL_AMBIENT],1,light.ambient);
		glUniform3fv(currentShader->vars[SHADER_UNI_LIGHT_COL_DIFFUSE],1,light.diffuse);
		glUniform3fv(currentShader->vars[SHADER_UNI_LIGHT_COL_SPECULAR],1,light.specula);
	}
}

void SCR_CheckErrorsF(char* step, char* details)
{
	GLenum err = glGetError();
	switch (err) {
		case GL_INVALID_ENUM:printf("Error GL_INVALID_ENUM %s, %s\n", step,details); break;
		case GL_INVALID_VALUE:printf("Error GL_INVALID_VALUE  %s, %s\n", step,details); break;
		case GL_INVALID_OPERATION:printf("Error GL_INVALID_OPERATION  %s, %s\n", step,details); break;				
		case GL_OUT_OF_MEMORY:printf("Error GL_OUT_OF_MEMORY  %s, %s\n", step,details); break;			
		case GL_NO_ERROR: break;
		default : printf("Error UNKNOWN  %s, %s \n", step,details);break;
	}
}

void LoadAllShaders(void)
{
	LoadProgram(&shaders[STRING_RENDER_SHADER], "data/shaders/v_text.glsl", "data/shaders/f_text.glsl", PROP_NULL);	
	shaders[STRING_RENDER_SHADER].vars[SHADER_ATT_VERTEX] = glGetAttribLocation(shaders[STRING_RENDER_SHADER].prog,"a_vertex");
	shaders[STRING_RENDER_SHADER].vars[SHADER_ATT_UV] = glGetAttribLocation(shaders[STRING_RENDER_SHADER].prog,"a_texcoord0");
	shaders[STRING_RENDER_SHADER].vars[SHADER_TEXT_COLOR_SAMPLER] = glGetUniformLocation(shaders[STRING_RENDER_SHADER].prog,"s_baseMap");
	shaders[STRING_RENDER_SHADER].vars[SHADER_MVT_MATRIX] = glGetUniformLocation(shaders[STRING_RENDER_SHADER].prog,"modelViewProjectionMatrix");
	
	LoadProgram(&shaders[SHADOW_GENERATOR_SHADER], "data/shaders/v_shadowMapGenerator.glsl", "data/shaders/f_shadowMapGenerator.glsl",PROP_NULL) ;	
	shaders[SHADOW_GENERATOR_SHADER].vars[SHADER_ATT_VERTEX] = glGetAttribLocation(shaders[SHADOW_GENERATOR_SHADER].prog,"a_vertex");
	shaders[SHADOW_GENERATOR_SHADER].vars[SHADER_MVT_MATRIX]= glGetUniformLocation(shaders[SHADOW_GENERATOR_SHADER].prog,"modelViewProjectionMatrix");

	memset(ubershaders, 0, sizeof(shader_prog_t*));
	//Uber shader instanciation will be loaded on the fly.
	
}



void Set3D(void)
{
	glClear (GL_COLOR_BUFFER_BIT |  GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	renderer.isBlending = 0;
}

void StopRendition(void)
{
	lastId = -1;
	lastBumpId = -1;
	lastSpecId = -1;
	
	currentShader = -1; ;
}

void UpLoadTextureToGPU(texture_t* texture)
{
	if (!texture || !texture->data || texture->textureId != 0)
		return;
	
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &texture->textureId);
	glBindTexture(GL_TEXTURE_2D, texture->textureId);
	
	if (texture->format == TEXTURE_GL_RGB ||texture->format == TEXTURE_GL_RGBA)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->width, texture->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->data);
	else
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, texture->format, texture->width,texture->height, 0, texture->dataLength, texture->data);
	
	
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, DE_DEFAULT_FILTERING);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, DE_DEFAULT_FILTERING);	
	

	if (texture->file != NULL)
		FS_CloseFile(texture->file);
		
	free(texture->data);	
	 
}

void SetTextures(material_t* material)
{
	unsigned int textureId;
	
	textureId = material->textures[TEXTURE_DIFFUSE]->textureId ;
	
	if (lastId != textureId)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,textureId);
		glUniform1i ( currentShader->vars[SHADER_TEXT_COLOR_SAMPLER], 0 );
		STATS_AddTexSwitch();
		lastId = textureId;
	}
	
	if ( (currentShader->props & PROP_BUMP) == PROP_BUMP)
	{
		textureId = material->textures[TEXTURE_BUMP]->textureId ;
		
		if (lastBumpId != textureId)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D,textureId);
			glUniform1i ( currentShader->vars[SHADER_TEXT_BUMP_SAMPLER], 1 );
			lastBumpId = textureId;
		}
	}
	
	if ( (currentShader->props & PROP_SPEC) == PROP_SPEC)
	{
		textureId = material->textures[TEXTURE_SPECULAR]->textureId ;
		
		if (lastSpecId != textureId)
		{
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D,textureId);
			glUniform1i ( currentShader->vars[SHADER_TEXT_SPEC_SAMPLER], 2 );
			lastSpecId = textureId;
		}
	}
	
}


void SetTexture(unsigned int textureId)
{
	if (lastId == textureId)
		return;
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,textureId);
	glUniform1i ( currentShader->vars[SHADER_TEXT_COLOR_SAMPLER], 0 );
	STATS_AddTexSwitch();
	lastId = textureId;
}

void RenderEntitiesMD5(void* md5Void)
{
	md5_object_t* md5Object;
	md5_mesh_t* currentMesh;
	int i;
	
	md5Object = (md5_object_t*)md5Void;
	
	
	/* Draw each mesh of the model */	
	for (i = 0; i < 1; i++)
    {
		currentMesh = &md5Object->md5Model.meshes[i];
		
		if (!renderer.isRenderingShadow)
		{
			//RenderNormalsF( currentMesh);
			glVertexAttribPointer(currentShader->vars[SHADER_ATT_NORMAL], 3, GL_SHORT, GL_TRUE,  sizeof(vertex_t), currentMesh->vertexArray->normal);
			glVertexAttribPointer(currentShader->vars[SHADER_ATT_TANGENT], 3, GL_SHORT, GL_TRUE,  sizeof(vertex_t), currentMesh->vertexArray->tangent);	
			glVertexAttribPointer(currentShader->vars[SHADER_ATT_UV], 2, GL_SHORT, GL_TRUE,  sizeof(vertex_t), currentMesh->vertexArray->text);			
			STATS_AddTriangles(currentMesh->num_tris);
		}
		glVertexAttribPointer(currentShader->vars[SHADER_ATT_VERTEX], 3, GL_FLOAT, GL_FALSE,  sizeof(vertex_t), currentMesh->vertexArray->pos);
		glDrawElements (GL_TRIANGLES, currentMesh->num_tris * 3, GL_UNSIGNED_SHORT, currentMesh->vertexIndices);
		
    }
}

void RenderEntitiesOBJ(void* objVoid)
{
	
	obj_t* obj;
	
	obj = (obj_t*)objVoid;
	
	if (!renderer.isRenderingShadow)
	{

		//RenderNormalsF( currentMesh);	
		glVertexAttribPointer(currentShader->vars[SHADER_ATT_TANGENT], 3, GL_SHORT, GL_TRUE,  sizeof(obj_vertex_t), obj->vertices[0].tangent);		
		glVertexAttribPointer(currentShader->vars[SHADER_ATT_NORMAL], 3, GL_SHORT, GL_TRUE,  sizeof(obj_vertex_t), obj->vertices[0].normal);
		glVertexAttribPointer(currentShader->vars[SHADER_ATT_UV], 2, GL_FLOAT, GL_FALSE,  sizeof(obj_vertex_t), obj->vertices[0].textCoo);	
		STATS_AddTriangles(obj->num_indices/3);
	}
	
	glVertexAttribPointer(currentShader->vars[SHADER_ATT_VERTEX], 3, GL_FLOAT, GL_FALSE,  sizeof(obj_vertex_t), obj->vertices[0].position);
	glDrawElements (GL_TRIANGLES, obj->num_indices, GL_UNSIGNED_SHORT, obj->indices);
	
	
}

void SetupCamera(void)
{
	vec3_t vLookat;
	
	vectorAdd(camera.position,camera.forward,vLookat);
	
	gluLookAt(camera.position, vLookat, camera.up, modelViewMatrix);
	
	gluPerspective(camera.fov, camera.aspect,camera.zNear, camera.zFar, projectionMatrix);
	
}


void ComputeInvModelMatrix(matrix_t matrix, matrix_t dest)
{
	
	matrix_t invTranslation;
	matrix_t invRotation;
	
	
	invTranslation[0] = 1 ;	invTranslation[4] = 0 ;	invTranslation[8] = 0 ;		invTranslation[12] = -matrix[12] ;
	invTranslation[1] = 0 ;	invTranslation[5] = 1 ;	invTranslation[9] = 0 ;		invTranslation[13] = -matrix[13] ;
	invTranslation[2] = 0 ;	invTranslation[6] = 0 ;	invTranslation[10] = 1 ;	invTranslation[14] = -matrix[14] ;
	invTranslation[3] = 0 ;	invTranslation[7] = 0 ;	invTranslation[11] = 0 ;	invTranslation[15] = 1 ;
	
	invRotation[0] = matrix[0] ;		invRotation[4] = matrix[1] ;		invRotation[8] = matrix[2] ;		invRotation[12] = 0 ;
	invRotation[1] = matrix[4] ;		invRotation[5] = matrix[5] ;		invRotation[9] = matrix[6] ;		invRotation[13] = 0 ;
	invRotation[2] = matrix[8] ;		invRotation[6] = matrix[9] ;		invRotation[10] = matrix[10] ;		invRotation[14] = 0 ;
	invRotation[3] = 0 ;				invRotation[7] = 0 ;				invRotation[11] = 0 ;				invRotation[15] = 1 ;
	
	
	matrix_multiply(invRotation, invTranslation, dest);
}

void RenderEntities(void)
{
	int i;
	entity_t* entity;
	matrix_t inv_modelMatrix;
	vec4_t modelSpaceLightPos;
	vec4_t modelSpaceCameraPos;
	matrix_t tmp;
	
	
	renderer.isRenderingShadow = ((renderer.props & PROP_SHADOW) == PROP_SHADOW) ;
	if (renderer.isRenderingShadow)
	{
		glCullFace(GL_FRONT);
		glBindFramebuffer(GL_FRAMEBUFFER,shadowFBOId);
		glClearColor(1.0,1.0,1.0,1.0);
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.0,0.0,0.0,1.0);
		SRC_UseShader(&shaders[SHADOW_GENERATOR_SHADER]);
		glEnableVertexAttribArray(currentShader->vars[SHADER_ATT_VERTEX] );
		glViewport(0, 0, renderWidth*shadowMapRation, renderHeight*shadowMapRation);
		
		//Setup perspective and camera
		gluPerspective(light.fov, camera.aspect,camera.zNear, camera.zFar, projectionMatrix);
		gluLookAt(light.position, light.lookAt, light.upVector, modelViewMatrix);
	
		entity = map;
		for(i=0; i < num_map_entities; i++,entity++)
		{		
			matrix_multiply(modelViewMatrix,entity->matrix, tmp);
			matrix_multiply(projectionMatrix,tmp, modelViewProjectionMatrix);
			glUniformMatrix4fv(currentShader->vars[SHADER_MVT_MATRIX]   ,1,GL_FALSE,modelViewProjectionMatrix);
			
			if (entity->type == ENTITY_OBJ)
				RenderEntitiesOBJ(entity->model);
			else
				RenderEntitiesMD5(entity->model);
			
			//Also need to cache the PVM matrix in entity
			matrixCopy(modelViewProjectionMatrix,entity->cachePVMShadow);
		}
		
		glViewport(0, 0, renderWidth, renderHeight);
		glCullFace(GL_BACK);
		
		renderer.isRenderingShadow = 0;
		
		glBindFramebuffer(GL_FRAMEBUFFER,renderer.mainFramebufferId);
	}
	
	
	
	//Setup perspective and camera
	SetupCamera();
	
	entity = map;
	
	for(i=0; i < num_map_entities; i++,entity++)
	{
		//if (entity->type != ENTITY_OBJ)
		//	continue;
		
		SRC_BindUberShader(entity->material->prop);
		
		matrix_multiply(modelViewMatrix,entity->matrix, tmp);
		matrix_multiply(projectionMatrix,tmp, modelViewProjectionMatrix);
		glUniformMatrix4fv(currentShader->vars[SHADER_MVT_MATRIX]   ,1,GL_FALSE,modelViewProjectionMatrix);
		
		ComputeInvModelMatrix(entity->matrix, inv_modelMatrix);
		
		matrix_transform_vec4t(inv_modelMatrix, light.position, modelSpaceLightPos);
		glUniform3fv(currentShader->vars[SHADER_UNI_LIGHT_POS],1,modelSpaceLightPos);
		
		matrix_transform_vec4t(inv_modelMatrix, camera.position, modelSpaceCameraPos);
		glUniform3fv(currentShader->vars[SHADER_UNI_CAMERA_POS],1,modelSpaceCameraPos);

		
		
		if (renderer.props & PROP_SHADOW == PROP_SHADOW)
			glUniformMatrix4fv(currentShader->vars[SHADER_LIGHTPOV_MVT_MATRIX]   ,1,GL_FALSE,entity->cachePVMShadow);
		
		
		SetTextures(entity->material);
		
		if (entity->material->hasAlpha )
		{
			if (!renderer.isBlending)
			{
				renderer.isBlending = 1;
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				STATS_AddBlendingSwitch();
			}
		}
		else
		{
			if (renderer.isBlending)
			{
				renderer.isBlending = 0;
				glDisable(GL_BLEND);
				STATS_AddBlendingSwitch();
			}
		}
		
		glUniform1f(currentShader->vars[SHADER_UNI_MATERIAL_SHININESS], entity->material->shininess);
		glUniform3fv(currentShader->vars[SHADER_UNI_MAT_COL_SPECULAR],1,entity->material->specularColor);
		
		
		if (entity->type == ENTITY_OBJ)
			RenderEntitiesOBJ(entity->model);
		else
			RenderEntitiesMD5(entity->model);
		
		
	}
	
}

void RenderString(svertex_t* vertices,ushort* indices, uint numIndices)
{	
	
	
	glVertexAttribPointer(currentShader->vars[SHADER_ATT_VERTEX], 2, GL_SHORT, GL_FALSE,  sizeof(svertex_t), vertices->pos);
	glVertexAttribPointer(currentShader->vars[SHADER_ATT_UV], 2, GL_FLOAT, GL_FALSE,  sizeof(svertex_t), vertices->text);			
	//TODO CHANGE glDrawElements
	glDrawElements (GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT,indices);
	
	//glDrawArrays(GL_TRIANGLES, 0, numVertices);
	STATS_AddTriangles(numIndices/3);
	
}

void SetupLighting(void)
{
	//vec3_t cameraSpaceLightPos;
	

	
	// Need to send the position in camera space, best practice is to use a normal matrix (transpose inverse) ....
	// But who fucking cares ?
//	matrix_multiplyVertexByMatrix(light.position,modelViewMatrix,cameraSpaceLightPos);
	
				
}


void Set2D(void)
{
	
	float t1,t2,t3;
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	
	
	
	t1 = -(renderWidth + -renderWidth)/ (float)(renderWidth  - -renderWidth);
	t2 = -(renderHeight+ -renderHeight)/(float)(renderHeight - -renderHeight);
	t3 = -(1 +  -1)/(1 - -1);
	
	modelViewProjectionMatrix[0] = 2.0f/ (renderWidth - - renderWidth )	; modelViewProjectionMatrix[4] = 0								; modelViewProjectionMatrix[8] = 0			; modelViewProjectionMatrix[12] =t1 ;
	modelViewProjectionMatrix[1] = 0								; modelViewProjectionMatrix[5] = 2.0f/(renderHeight - -renderHeight); modelViewProjectionMatrix[9] = 0			; modelViewProjectionMatrix[13] = t2 ;
	modelViewProjectionMatrix[2] = 0								; modelViewProjectionMatrix[6] = 0									; modelViewProjectionMatrix[10] = -2.0f/(1 - -1); modelViewProjectionMatrix[14] = t3;
	modelViewProjectionMatrix[3] = 0								; modelViewProjectionMatrix[7] = 0									; modelViewProjectionMatrix[11] = 0			; modelViewProjectionMatrix[15] = 1  ;	
	
	SRC_UseShader(&shaders[STRING_RENDER_SHADER]);
	glEnableVertexAttribArray(currentShader->vars[SHADER_ATT_UV]);
	glEnableVertexAttribArray(currentShader->vars[SHADER_ATT_VERTEX] );
	//matrixLoadIdentity(modelViewProjectionMatrix);
	glUniformMatrix4fv(currentShader->vars[SHADER_MVT_MATRIX],1,GL_FALSE,modelViewProjectionMatrix);
	
	//glBindFramebuffer(GL_FRAMEBUFFER,0);
	//SetTexture(2);
	
	/*
	//Draw sprite(textureId, vertices, indices);	
	SetTexture(1);
	glVertexAttribPointer(currentShader->vars[SHADER_ATT_VERTEX], 2, GL_SHORT, GL_FALSE,  sizeof(svertex_t), verticesSprite->pos);
	glVertexAttribPointer(currentShader->vars[SHADER_ATT_UV], 2, GL_FLOAT, GL_FALSE,  sizeof(svertex_t), verticesSprite->text);			
	//TODO CHANGE glDrawElements
	glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,indicesSprite);
	 */
}

void RenderSprite(uint textureId, svertex_t* verticesSprite, uint* indicesSprite, uint numIndices)
{
	SetTexture(textureId);
	glVertexAttribPointer(currentShader->vars[SHADER_ATT_VERTEX], 2, GL_SHORT, GL_FALSE,  sizeof(svertex_t), verticesSprite->pos);
	glVertexAttribPointer(currentShader->vars[SHADER_ATT_UV], 2, GL_FLOAT, GL_FALSE,  sizeof(svertex_t), verticesSprite->text);			
	//TODO CHANGE glDrawElements
	glDrawElements (GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT,indicesSprite); //numIndices==6
}

void GetColorBuffer(uchar* data)
{
	glReadPixels(0,0,renderWidth,renderHeight,GL_RGBA, GL_UNSIGNED_BYTE,data);
}


void initProgrRenderer(renderer_t* renderer)
{
	/*
	int numAttVert;
	int numUnifVert, numUnifFrag;
	int numVary;
	*/
	
	SCR_CheckErrorsF("initProgrRenderer","438");
	
	renderer->type = GL_20_RENDERER ;
	
	renderer->props |= PROP_BUMP;
	renderer->props |= PROP_SPEC;
	renderer->props |= PROP_DIFF;
	renderer->props |= PROP_SHADOW;

	renderer->Set3D = Set3D;
	renderer->StopRendition = StopRendition;
//	renderer->PushMatrix = PushMatrix;
//	renderer->PopMatrix = PopMatrix;
	renderer->SetTexture = SetTexture;
	renderer->RenderEntities = RenderEntities;
	renderer->UpLoadTextureToGpu = UpLoadTextureToGPU;
	renderer->Set2D = Set2D;
	renderer->RenderString = RenderString;
	renderer->GetColorBuffer = GetColorBuffer;
	renderer->RenderSprite = RenderSprite;

	
	glViewport(0, 0, renderWidth, renderHeight);
	

	
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_BLEND);
	glClearColor(0, 0, 0,1.0f);
	
	//Need to load all shaders here
	/*
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numAttVert);
	
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &numUnifVert);	
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &numUnifFrag);

	glGetIntegerv(GL_MAX_VARYING_VECTORS, &numVary);

	printf("GL_MAX_VERTEX_ATTRIBS = %d \n",numAttVert);
	printf("GL_MAX_VERTEX_UNIFORM_VECTORS = %d \n",numUnifVert);
	printf("GL_MAX_FRAGMENT_UNIFORM_VECTORS = %d \n",numUnifFrag);
	printf("GL_MAX_VARYING_VECTORS = %d \n",numVary);
	*/
	LoadAllShaders();
	
	//Create shadowMap and FBO
	CreateFBOandShadowMap();
	
	
	SCR_CheckErrorsF("End of initProgrRenderer", "no details");

}