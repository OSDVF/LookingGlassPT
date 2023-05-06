//!#version 430
#extension GL_ARB_bindless_texture: enable

#define PI 3.141592653589
#ifndef MAX_BOUNCES
#define MAX_BOUNCES 3
#endif

#ifdef FLAT_SCREEN
#define getRay getFlatScreenRay
#else
#define getRay getLookingGlassRay
#endif

#ifndef MAX_OBJECT_BUFFER
#define MAX_OBJECT_BUFFER 64
#endif

out vec4 OutColor;
in vec2 vNDCpos;

layout(shared, binding = 0)
uniform CalibrationBuffer {
    float pitch;
    float tilt;
    float center;
    float subp;
    vec2 resolution;
} uCalibration;

struct ObjectDefinition {
    uint material;
    uint vboStartIndex;
    uint vertexAttrs;
    uint totalAttrsSize;
    float aabbMinX;
    float aabbMinY;
    float aabbMinZ;
    float aabbMaxX;
    float aabbMaxY;
    float aabbMaxZ;
};

struct Light {
    vec3 position;
    float size; //Dimension in both axes
    vec3 normal;
    float area; // size squared
    vec4 color;
    uint object;
};

layout(shared, binding = 1) uniform ObjectBuffer {
    ObjectDefinition[MAX_OBJECT_BUFFER] objectDefinitions;
};

layout(binding = 2, rgba8)
uniform image2D uScreenAlbedo;
layout(binding = 3, rgba8)
uniform image2D uScreenNormal;
layout(binding = 4, rgba16f)
uniform image2D uScreenColorDepth;

uniform uint uObjectCount = 0;
uniform float uTime = 0;
uniform vec2 uWindowSize = vec2(500,500);
uniform vec2 uWindowPos;
uniform vec2 uMouse = vec2(0.5,0.5);
uniform float uViewCone = radians(20); // 40 deg in rad
uniform float uFocusDistance = 10;

uniform float uInvRayCount = 1.;
uniform uint uRayIndex = 0;
uniform float uRayOffset = 1e-5;
uniform uint uSubpI = 0;
uint subpI = uSubpI;

layout(std430, binding = 5) readonly buffer AttributeBuffer {
    float[] dynamicVertexAttrs;
};
struct Triangle {
    vec3 v0;
    vec3 edgeA;

    vec3 edgeB;

    uvec3 attributeIndices;
};

struct TriangleSecondHalf {
    vec3 edgeB;
    uint padding;
    uvec3 attributeIndices;
    uint objectIndex;
};

struct Material {
    uint isTexture;
    // samplerOrColor is not defined as uvec2 because GL would perform some weird memory layout
    uint samplerOrColorX;
    uint samplerOrColorY;
    float colorZ;
    uint samplerOrEmissionX;
    uint samplerOrEmissionY;
    float emissionZ;
    float transparency;
};

 // unpacked node
struct BVHNode
{
	vec4 bboxMin;
	vec4 bboxMax;
};

layout(std430, binding = 6) readonly buffer TriangleBuffer {
    TriangleSecondHalf[] trianglesSecond;
};

layout(std430, binding = 7) readonly buffer MaterialBuffer {
    Material[] materials;
};

layout(std430, binding = 8) readonly buffer LightBuffer {
    Light[] lights;
};

layout(std430, binding = 9) readonly buffer BVHBuffer {
    vec4[] bvh;//packed nodes and vertices at the leaf level
};


uniform mat4 uView;
uniform mat4 uProj;
float cameraFarPlane = uProj[2].w / (uProj[2].z + 1.0);

struct Ray {
    vec3 origin;
    vec3 direction;
};

//const float pos_infinity = uintBitsToFloat(0x7F800000);
const float pos_infinity = 100000.0;
const float neg_infinity = uintBitsToFloat(0xFF800000);
struct Hit {
    uint vboStartIndex;
    uint attrs;
    uint totalAttrSize;
    uint material;
    float rayT;
    vec2 barycentric;
    uvec3 indices;
    vec3 normal;
};

// https://www.shadertoy.com/view/4lfcDr
vec2
sample_disk(vec2 uv)
{
	float theta = 2.0 * 3.141592653589 * uv.x;
	float r = sqrt(uv.y);
	return vec2(cos(theta), sin(theta)) * r;
}

// Cosine-weighted sampling.
vec3
sample_cos_hemisphere(vec2 uv)
{
	vec2 disk = sample_disk(uv);
	return vec3(disk.x, sqrt(max(0.0, 1.0 - dot(disk, disk))), disk.y);
}

mat3
construct_ONB_frisvad(vec3 normal)
{
	mat3 ret;
	ret[1] = normal;
	if(normal.z < -0.999805696) {
		ret[0] = vec3(0.0, -1.0, 0.0);
		ret[2] = vec3(-1.0, 0.0, 0.0);
	}
	else {
		float a = 1.0 / (1.0 + normal.z);
		float b = -normal.x * normal.y * a;
		ret[0] = vec3(1.0 - normal.x * normal.x * a, b, -normal.x);
		ret[2] = vec3(b, 1.0 - normal.y * normal.y * a, -normal.y);
	}
	return ret;
}
uint seed = uint(gl_FragCoord.x + gl_FragCoord.y * gl_FragCoord.x);
void
encrypt_tea(inout uvec2 arg)
{
	uvec4 key = uvec4(0xa341316c, 0xc8013ea4, 0xad90777d, 0x7e95761e);
	uint v0 = arg[0], v1 = arg[1];
	uint sum = 0u;
	uint delta = 0x9e3779b9u;

	for(int i = 0; i < 32; i++) {
		sum += delta;
		v0 += ((v1 << 4) + key[0]) ^ (v1 + sum) ^ ((v1 >> 5) + key[1]);
		v1 += ((v0 << 4) + key[2]) ^ (v0 + sum) ^ ((v0 >> 5) + key[3]);
	}
	arg[0] = v0;
	arg[1] = v1;
}

vec2
get_random()
{
  	uvec2 arg = uvec2(uTime, seed++);
  	encrypt_tea(arg);
  	return fract(vec2(arg) / vec2(0xffffffffu));
}

Ray createSecondaryRay(vec2 coord, vec3 pos, vec3 normal)
{
    vec2 randomValues = get_random();
    mat3 onb = construct_ONB_frisvad(normal);
    vec3 dir = normalize(onb * sample_cos_hemisphere(randomValues));
    Ray ray_next = Ray(pos, dir);
	ray_next.origin += normal * uRayOffset;//Offset to prevent self-blocking
    return ray_next;
}

const int tile = 45;
Ray generateChaRay(){
    vec2 texCoords = vNDCpos*.5f+.5f;
    
	float view = (texCoords.x + uCalibration.subp * subpI + texCoords.y * uCalibration.tilt) * uCalibration.pitch - uCalibration.center;
	view = fract(view);
	view = (1.0 - view);
	vec2 vvPos = texCoords*2.f-1.f;

    view = floor(view * tile);

    mat4 newView = uView;
    mat4 newProj = uProj;
  
    float aspect = uWindowSize.x/uWindowSize.y;
    float ttt = view / (45.f - 1);
    float S = 0.5f*uFocusDistance*tan(uViewCone);
    float s = S-2*ttt*S;
    float invTanFov = uProj[1][1];

    newView[3][0] += s;
    newProj[2][0] += s/(uFocusDistance*aspect*(1/invTanFov));

    vec4 dir = inverse(newProj * newView)*vec4(vvPos,1,1);
    dir.xyz/=dir.w;
    vec3 pos = vec3(inverse(newView)*vec4(0,0,0,1));

    Ray ray;
    ray.origin=pos;
    ray.direction=normalize(dir.xyz);
    return ray;
}

void getLookingGlassRay(vec2 pix, out Ray ray) {
    ray = generateChaRay();
}

void getFlatScreenRay(vec2 pix, out Ray ray){
  vec4 dir = inverse(uProj * uView)*vec4(pix,1,1);
  dir.xyz/=dir.w;
  vec3 pos = vec3(inverse(uView)*vec4(0,0,0,1));

  ray = Ray(pos, normalize(dir.xyz));
}

bool rayBoxIntersection(vec3 minPos, vec3 maxPos, Ray ray)
{
    vec3 invDir = 1.0/ray.direction;
    vec3 t1 = (minPos - ray.origin)*invDir;
    vec3 t2 = (maxPos - ray.origin)*invDir;

    float tmin = max(
            min(t1.x, t2.x), 
            max(
                min(t1.y, t2.y), min(t1.z, t2.z)
            )
        );
    float tmax = min(
            max(t1.x, t2.x),
            min(
                max(t1.y, t2.y), max(t1.z, t2.z)
            )
        );

    if(tmax < 0 || tmin > tmax)
        return false;

    return true;
}

// Extract the sign bit from a 32-bit floating point number.
float signmsk(float x ){
    return intBitsToFloat( floatBitsToInt(x) & 0x80000000 );
}

// Exclusive-or the bits of two floating point numbers together, interpreting
// the result as another floating point number.
float xorf(float x, float y)
{
    return intBitsToFloat( floatBitsToInt(x) ^ floatBitsToInt(y) );
}

const float tNear = 0.01;
const float tFar = 1000;
//https://github.com/embree/embree/blob/master/kernels/geometry/triangle_intersector_moeller.h
bool embreeIntersect(const Triangle tri, const Ray ray,
    inout float T, out float U, out float V, out vec3 normal){
    vec3 edgeA = tri.edgeB;
    vec3 edgeB = tri.edgeA;
    normal = cross(tri.edgeA, tri.edgeB);
    const float den = dot(normal, ray.direction);
    #ifdef CULLING
    if(den < 0.001)
    {
        return false;
    }
    #endif
    vec3 C = tri.v0 - ray.origin;
    
    vec3 R = cross(ray.direction, C);
    
    const float absDen = abs( den );
    #ifndef CULLING
    if(absDen < 0.001)
    {
        return false;
    }
    #endif
    const float sgnDen = signmsk( den );
    
    // perform edge tests
    U = xorf( dot( R, edgeB ), sgnDen );
    if( U < 0)
    {
        return false;
    }
    V = xorf( dot( R, edgeA ), sgnDen );
    if( V < 0)
    {
        return false;
    }
    
    // No backface culling in this experiment
    if ( U+V > absDen ) return false;
    
    // perform depth test
    float invDen = 1/absDen;
    float newT = xorf( dot( normal, C ), sgnDen ) * invDen;
    if(newT >= tNear && newT < T)
    {
        T = newT;
        U *= invDen;
        V *= invDen;
        return true;
    }
    return false;
}
 
 float interpolateAttribute(in uint vboStartIndex, in vec2 barycentric, in uint totalAttrSize, in uint currentAttrOffset, in uvec3 indices)
 {
    return dynamicVertexAttrs[
            vboStartIndex
            + indices.x * totalAttrSize
            + currentAttrOffset
        ] * (1 - barycentric.x - barycentric.y) +
        dynamicVertexAttrs[
            vboStartIndex
            + indices.y * totalAttrSize
            + currentAttrOffset
        ] * barycentric.x +
        dynamicVertexAttrs[
            vboStartIndex
            + indices.z * totalAttrSize
            + currentAttrOffset
        ] * barycentric.y;
    
 }

vec3 interpolate3(in uint vboStartIndex, in vec2 barycentric, in uint totalAttrSize, in uint currentAttrOffset, uvec3 indices)
{
    vec3 result;
    for(uint attrPart = 0; attrPart < 3; attrPart++)
    {
        result[attrPart] = interpolateAttribute(vboStartIndex, barycentric, totalAttrSize, currentAttrOffset + attrPart, indices);
    }
    return result;
}

vec4 interpolate4(in uint vboStartIndex, in vec2 barycentric, in uint totalAttrSize, in uint currentAttrOffset, uvec3 indices)
{
    vec4 result;
    for(uint attrPart = 0; attrPart < 4; attrPart++)
    {
        result[attrPart] = interpolateAttribute(vboStartIndex, barycentric, totalAttrSize, currentAttrOffset + attrPart, indices);
    }
    return result;
}

vec2 interpolate2(in uint vboStartIndex, in vec2 barycentric, in uint totalAttrSize,
                    in uint currentAttrOffset, in uvec3 indices)
{
    vec2 result;
    for(uint attrPart = 0; attrPart < 2; attrPart++)
    {
        result[attrPart] = interpolateAttribute(vboStartIndex, barycentric, totalAttrSize, currentAttrOffset + attrPart, indices);
    }
    return result;
}

vec3 getMaterialColor(uint materialIndex, out vec3 emission, vec2 uv)
{
    Material mat = materials[materialIndex];
    vec3 albedo = vec3(0);
    if((mat.isTexture & 1u) != 0)
    {
        // Has albedo
        // This is effectively uint64_t
        sampler2D samp = sampler2D(uvec2(mat.samplerOrColorY, mat.samplerOrColorX));
        // Return a color from a bindless texture
        albedo = texture(samp, uv).xyz;
    }
    else
    {
        albedo = vec3(uintBitsToFloat(uvec2(mat.samplerOrColorX, mat.samplerOrColorY)), mat.colorZ);
    }
    if((mat.isTexture & 2u) != 0)
    {
        // Has emission
        sampler2D samp = sampler2D(uvec2(mat.samplerOrEmissionY, mat.samplerOrEmissionX));
        emission = texture(samp, uv).xyz;
    }
    else
    {
        emission = vec3(uintBitsToFloat(uvec2(mat.samplerOrEmissionX,mat.samplerOrEmissionY)), mat.emissionZ);   
    }
    return albedo;
}

//https://www.shadertoy.com/view/tdBXRW
float xorTextureGradBox( in vec2 pos, in vec2 ddx, in vec2 ddy )
{
    float xor = 0.0;
    for( int i=0; i<8; i++ )
    {
        // filter kernel
        vec2 w = max(abs(ddx), abs(ddy)) + 0.01;  
        // analytical integral (box filter)
        vec2 f = 2.0*(abs(fract((pos-0.5*w)/2.0)-0.5)-abs(fract((pos+0.5*w)/2.0)-0.5))/w;
        // xor pattern
        xor += 0.5 - 0.5*f.x*f.y;
        
        // next octave        
        ddx *= 0.5;
        ddy *= 0.5;
        pos *= 0.5;
        xor *= 0.5;
    }
    return xor;
}

void testVisibility(Ray ray, inout Hit closestHit)
{
    // Traverse BVH
    // Adapted from https://github.com/kayru/RayTracedShadows/blob/master/Source/Shaders/RayTracedShadows.comp
    uint nodeIndex = 0;

    uint lastNode = bvh.length();
     
	while(nodeIndex < lastNode)
	{
        BVHNode node;
        node.bboxMin = bvh[nodeIndex * 2];
        node.bboxMax = bvh[nodeIndex * 2 + 1];

        uint primitiveIndex = floatBitsToUint(node.bboxMin.w);

        bool isLeaf = primitiveIndex != 0xFFFFFFFF;
        if(isLeaf)
        {
            TriangleSecondHalf triSecond = trianglesSecond[primitiveIndex];
            Triangle tri = Triangle(node.bboxMin.xyz,node.bboxMax.xyz, triSecond.edgeB, triSecond.attributeIndices);
            float outT, outU, outV;
            vec3 normal;
            if(embreeIntersect(
                tri,
                ray,
                closestHit.rayT, outU, outV, normal))
            //if(rayTriangleIntersect(ray.origin, ray.direction, tri.v0, tri.v0 - vec3(tri.edgeAx,tri.edgeAy,tri.edgeAz), vec3(tri.edgeBx,tri.edgeBy,tri.edgeBz) + tri.v0, outT, outV, outU))
            {
                ObjectDefinition obj = objectDefinitions[triSecond.objectIndex];
                closestHit.vboStartIndex = obj.vboStartIndex;
                closestHit.attrs = obj.vertexAttrs;
                closestHit.material = obj.material;
                closestHit.totalAttrSize = obj.totalAttrsSize;
                closestHit.barycentric = vec2(outV, outU);
                closestHit.indices = tri.attributeIndices;
                closestHit.normal = normalize(normal);
                return;
            }
        }
        else if (rayBoxIntersection(node.bboxMin.xyz, node.bboxMax.xyz, ray))
		{
			// If the ray intersects the bounding box
            ++nodeIndex;
            continue;
        }

        nodeIndex = floatBitsToUint(node.bboxMax.w);
    }
}

bool resolveRay(Ray ray, float far, out vec3 albedo, out vec3 normal, out vec3 emission, out float depth)
{
    Hit closestHit;
    closestHit.rayT = far;
    testVisibility(ray, closestHit);
    if(closestHit.rayT != far)
    {
        // Interpolate other triangle attributes by barycentric coordinates
        uint currentAttrOffset = 0;
        vec3 surfaceNormal = normalize(closestHit.normal);
        vec3 materialColor = vec3(1., 1., 1.);
        vec4 vertexColor = vec4(1., 1., 1., 0.);
        vec2 uv = vec2(0., 0.);
        if ((closestHit.attrs & 1u) != 0)
        {
            //Has vertex colors
            vertexColor = interpolate4(closestHit.vboStartIndex, closestHit.barycentric, closestHit.totalAttrSize, currentAttrOffset, closestHit.indices);;
            currentAttrOffset += 4;
        }
        if ((closestHit.attrs & 2u) != 0)
        {
            // Has normals
            surfaceNormal = normalize(interpolate3(closestHit.vboStartIndex, closestHit.barycentric, closestHit.totalAttrSize, currentAttrOffset, closestHit.indices));
            currentAttrOffset += 3;
        }
        if ((closestHit.attrs & 4u) != 0)
        {
            // Has uvs
            uv = interpolate2(closestHit.vboStartIndex, closestHit.barycentric, closestHit.totalAttrSize, currentAttrOffset, closestHit.indices);
            currentAttrOffset += 2;
        }
        materialColor = getMaterialColor(closestHit.material, emission, uv);

        albedo = materialColor;//mix(materialColor, vertexColor.rgb, vertexColor.a);
        normal = surfaceNormal;
        depth = closestHit.rayT;
        return true;
    }
    return false;
}
bool resolveRay(Ray ray, out vec3 albedo, out vec3 normal, out vec3 emission, out float depth)
{
    return resolveRay(ray, cameraFarPlane, albedo, normal, emission, depth);
}

void updateGBuffer(vec3 albedo, vec3 emission, vec3 normal, float depth, ivec2 coord)
{
    imageStore(uScreenAlbedo, coord, vec4(albedo + emission, (emission.x + emission.y)*0.55));//Albedo and emission factor
    imageStore(uScreenNormal, coord, vec4(normal * 0.5 + 0.5, emission.z));
    imageStore(uScreenColorDepth, coord, vec4(vec3(0.), depth));
}

vec3 sample_light(vec2 rng, Light light)
{
	return light.position + vec3(rng.x - 0.5, 0, rng.y - 0.5) * light.size;
}

vec3 getPlaneColor(Ray ray, float t)
{
    vec3 pos = ray.origin + t * ray.direction;
	return vec3(xorTextureGradBox(pos.xz,dFdx(pos.xz),dFdy(pos.xz)) / clamp(t*0.09, 2.1, 8.0));
}

vec3 rayTraceSubPixel(vec2 ndcCoord) {
    Ray primaryRay;
    getRay(ndcCoord, primaryRay);
    ivec2 coord = ivec2(gl_FragCoord);
    vec3 albedo, normal, emission;
    float depth;

    // raytrace the coordinate 'xor' plane at (0,-1,0)
	float planeDist = (-1-primaryRay.origin.y)/primaryRay.direction.y;

    if(uRayIndex == 0)
    {
        // This is primary ray
        if(resolveRay(primaryRay, albedo, normal, emission, depth))
        {
            updateGBuffer(albedo, emission, normal, depth, coord);
            if(planeDist > 0 && depth > planeDist)
            {
                // The object is behind the plane so make the plane 80% transparent
                return mix(albedo + emission, getPlaneColor(primaryRay, planeDist), 0.3);
            }
            return albedo + emission;
        }
        else
        {
            updateGBuffer(vec3(0), vec3(0), vec3(0), 0, coord);
            if(planeDist > 0)
            {
                return getPlaneColor(primaryRay, planeDist);
            }
            return vec3(0.1);
        }
    }
    else
    {
        // This is a secondary ray
        vec4 prevAlbedoEmission = imageLoad(uScreenAlbedo, coord);
        vec3 prevAlbedo = prevAlbedoEmission.rgb;
        vec3 albedo = prevAlbedo;
        vec4 previousNormalEmission = imageLoad(uScreenNormal, coord);
        vec3 previousNormal = previousNormalEmission.rgb * 2. - 1;
        vec3 emission;

        vec4 prevColorDepth = imageLoad(uScreenColorDepth, coord);
        float depth = prevColorDepth.a;
        vec3 contrib = prevColorDepth.rgb;
        // Return only light color for emissive materials
        if(previousNormalEmission.a > 0)
        {
            // The primary ray hit a light source here
            contrib += albedo;
        }
        else
        {
            vec3 throughput = albedo;

            vec3 secondaryColor;
            // Basically the alrogithm from https://www.shadertoy.com/view/4lfcDr
            for(int i = 0; i < MAX_BOUNCES; i++)
            {
                Light light = lights[0];
                vec3 position = primaryRay.origin + primaryRay.direction * depth;
                { // Next event estimation (sample light)
			        vec3 pos_ls = sample_light(get_random(), light);
                    vec3 dirToLight = pos_ls - position;
			        vec3 l_nee = dirToLight;
			        float rr_nee = dot(l_nee, l_nee);
			        l_nee /= sqrt(rr_nee);
			        float G = max(0.0, dot(previousNormal, l_nee)) * max(0.0, -dot(l_nee, light.normal)) / rr_nee;

			        if(G > 0.0) {
				        float light_pdf = 1.0 / (light.area * G);
				        float brdf_pdf = 1.0 / PI;

				        float w = light_pdf / (light_pdf + brdf_pdf);

				        vec3 brdf = albedo.rgb / PI;

                        // Test light visibility
                        float far = length(dirToLight);
                        Ray shadowRay = Ray(position, dirToLight / far);
                        Hit closestHit;
                        closestHit.rayT = far;//constant far plane 
                        testVisibility(shadowRay, closestHit);
                        if(closestHit.rayT == far) {
					        vec3 Le = light.color.xyz;
					        contrib += throughput * (Le * w * brdf) / light_pdf;
				        }
			        }
		        }
                { // Sample surface using BRDF
                    Ray secondary = createSecondaryRay(ndcCoord, position, previousNormal);
                    if(resolveRay(secondary, secondaryColor, normal, emission, depth))
                    {
			            vec3 brdf = secondaryColor.rgb / PI;

			            float brdf_pdf = 1.0 / PI;

			            if(emission.x > 0. || emission.y > 0. || emission.z > 0.) {
                            // Hit a light source
				            float G = max(0.0, dot(secondary.direction, previousNormal)) * max(0.0, -dot(secondary.direction, normal)) / (depth * depth);
				            if(G <= 0.0) // hit back side of light source
					            break;

				            float light_pdf = 1.0 / (light.area * G);

				            float w = brdf_pdf / (light_pdf + brdf_pdf);

				            vec3 Le = light.color.rgb;
				            contrib += throughput * (Le * w * brdf) / brdf_pdf;
				            break;
			            }

			            throughput *= brdf / brdf_pdf;

			            primaryRay = secondary;
			            previousNormal = normal;
			            albedo = secondaryColor;
                    }
                    else
                    {
                        break;
                    }
		        }
            }
        }
        imageStore(uScreenColorDepth, coord, vec4(contrib, prevColorDepth.a));
        return contrib
        #if !defined(SUBPIXEL_ONE_PASS) || defined(FLAT_SCREEN)
        * 3
        #endif
        * uInvRayCount;
    }
}

void main() {
    vec3 col = vec3(0);
    #if defined(SUBPIXEL_ONE_PASS) && !defined(FLAT_SCREEN)
    for(subpI = 0; subpI < 3; subpI++)
    {
        col[subpI] = rayTraceSubPixel(vNDCpos)[subpI];
    }
    #else
    col = rayTraceSubPixel(vNDCpos);
    #endif
	
    // gamma correction
	OutColor = vec4( pow(col, vec3(1.0 / 2.2)), 1.0 );
}