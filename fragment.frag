//!#version 430
#extension GL_ARB_bindless_texture: enable
//#extension GL_ARB_gpu_shader_int64 : enable

#define PI 3.141592653589
#ifndef NUM_BOUNCES
#define NUM_BOUNCES 3
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

// Layout is like this:
// uint material number (if higher than 2�147�483�648, the material is treated as color-only)
// uint index of first index in index buffer
// uint number of indices
// uint num of vertex attrs
// uint total attrs size
// uint 1st attr width
// uint 2nd attr width...

struct ObjectDefinition {
    uint material;
    uint vboStartIndex;
    uint firstTriangle;
    uint triNumber;
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
uniform image2D uScreenAlbedoDepth;
layout(binding = 3, rgba8)
uniform image2D uScreenNormalDepth;

uniform uint uObjectCount = 0;
uniform float uTime = 0;
uniform vec2 uWindowSize = vec2(500,500);
uniform vec2 uWindowPos;
uniform vec2 uMouse = vec2(0.5,0.5);
uniform float uViewCone = radians(20); // 40 deg in rad
uniform float uFocusDistance = 10;
uniform uvec4 uRayParameters = uvec4(
    0,
    1,
    1,
    floatBitsToUint(1.)
);

layout(std430, binding = 4) readonly buffer AttributeBuffer {
    float[] dynamicVertexAttrs;
};

struct Triangle {
    vec3 v0;
    float edgeAx;

    float edgeAy;
    float edgeAz;
    float edgeBx;
    float edgeBy;

    float edgeBz;
    float normalX;
    float normalY;
    float normalZ;

    uvec4 attributeIndices;
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

layout(std430, binding = 5) readonly buffer TriangleBuffer {
    Triangle[] triangles;
};

layout(std430, binding = 6) readonly buffer MaterialBuffer {
    Material[] materials;
};

layout(std430, binding = 7) readonly buffer LightBuffer {
    Light[] lights;
};

//TODO vyzkou�et jin� typy projekce, kter� budou generovat m�n� artefakt�

uniform mat4 uView;
uniform mat4 uProj;

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

int subpI;

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

vec2 get_random(vec2 coord, vec3 pos)
{
    return vec2(uintBitsToFloat(uRayParameters.y) * coord.x * pos.x,uintBitsToFloat(uRayParameters.z) * coord.y * pos.y);
}

Ray createSecondaryRay(vec2 coord, vec3 pos, vec3 normal)
{
    vec2 randomValues = get_random(coord, pos);
    mat3 onb = construct_ONB_frisvad(normal);
    vec3 dir = normalize(onb * sample_cos_hemisphere(randomValues));
    Ray ray_next = Ray(pos, dir);
	ray_next.origin += normal * 0.01 * dir;//Offset to prevent self-blocking
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
    // https://tavianator.com/2011/ray_box.html
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
#define MAGIC
//#define CULLING
bool embreeIntersect(const Triangle tri, const Ray ray,
    inout float T, out float U, out float V){
    vec3 edgeA = vec3(tri.edgeBx,tri.edgeBy,tri.edgeBz);
    vec3 edgeB = vec3(tri.edgeAx,tri.edgeAy,tri.edgeAz);
    vec3 normal = vec3(tri.normalX,tri.normalY,tri.normalZ);
    #ifdef MAGIC
    const float den = dot( normal, ray.direction );
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
    #else
    vec3 pvec = cross(ray.direction, edgeB);
    float det = dot(edgeA, pvec);
    if (abs(det) < 0.001) return false;
    
    float iDet = 1./det;
    
    vec3 tvec = ray.origin - tri.v0;
    U = dot(tvec, pvec) * iDet;
    float inside = step(0.0, U) * (1.-step(1.0, U));
    
    vec3 qvec = cross(tvec, edgeA);
    V = dot(ray.direction, qvec) * iDet;
    inside *= step(0.0, V) * (1. - step(1., U+V));
    if (inside == 0.0) return false;
    
    float d = dot(edgeB, qvec) * iDet;
    
    float f = step(0.0, -d);
    d = d * (1.-f) + (f * pos_infinity);    
    if (d > T) { return false; }
    
    T = d;
    return true;
    #endif
}


//#define MOLLER_TRUMBORE
const float kEpsilon = 1e-8; 
bool rayTriangleIntersect( 
    const vec3 orig, const vec3 dir, 
    const vec3 v0, const vec3 v1, const vec3 v2, 
    out float t, out float u, out float v) 
{ 
#ifdef MOLLER_TRUMBORE 
    vec3 v0v1 = v1 - v0; 
    vec3 v0v2 = v2 - v0; 
    vec3 pvec = cross(dir, v0v2); 
    float det = dot(v0v1, pvec); 
#ifdef CULLING 
    // if the determinant is negative the triangle is backfacing
    // if the determinant is close to 0, the ray misses the triangle
    if (det < kEpsilon) 
    {
        return false; 
    }
#else 
    // ray and triangle are parallel if det is close to 0
    if (abs(det) < kEpsilon)
    {
        return false; 
    }
#endif 
    float invDet = 1 / det; 
 
    vec3 tvec = orig - v0; 
    u = dot(tvec, pvec) * invDet; 
    if (u < 0 || u > 1)
    {
        return false;
    }
 
    vec3 qvec = cross(tvec, v0v1); 
    v = dot(dir, qvec) * invDet; 
    if (v < 0 || u + v > 1)
    {
        return false;
    }
 
    t = dot(v0v2, qvec) * invDet; 
 
    return true; 
#else 
    // compute plane's normal
    vec3 v0v1 = v1 - v0; 
    vec3 v0v2 = v2 - v0; 
    // no need to normalize
    vec3 N = cross(v0v1, v0v2);  //N 
    float denom = dot(N, N); 
 
    // Step 1: finding P
 
    // check if ray and plane are parallel ?
    float NdotRayDirection = dot(N, dir); 
 
    if (abs(NdotRayDirection) < kEpsilon)  //almost 0 
        return false;  //they are parallel so they don't intersect ! 
 
    // compute d parameter using equation 2
    float d = -dot(N,v0); 
 
    // compute t (equation 3)
    float newT = -(dot(N, orig) + d) / NdotRayDirection; 
 
    // check if the triangle is in behind the ray
    if (newT < 0 || newT > t) return false;  //the triangle is behind 
    t = newT;
 
    // compute the intersection point using equation 1
    vec3 P = orig + t * dir; 
 
    // Step 2: inside-outside test
    vec3 C;  //vector perpendicular to triangle's plane 
 
    // edge 0
    vec3 edge0 = v1 - v0; 
    vec3 vp0 = P - v0; 
    C = cross(edge0, vp0); 
    if (dot(N, C) < 0) return false;  //P is on the right side 
 
    // edge 1
    vec3 edge1 = v2 - v1; 
    vec3 vp1 = P - v1; 
    C = cross(edge1, vp1); 
    if ((u = dot(N, C)) < 0)  return false;  //P is on the right side 
 
    // edge 2
    vec3 edge2 = v0 - v2; 
    vec3 vp2 = P - v2; 
    C = cross(edge2, vp2); 
    if ((v = dot(N, C)) < 0) return false;  //P is on the right side; 
 
    u /= denom; 
    v /= denom; 
 
    return true;  //this ray hits the triangle 
#endif 
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
    bool isTexture;
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

void testVisibility(ObjectDefinition obj, Ray ray, inout Hit closestHit)
{
    // If the ray intersects the bounding box
    if(rayBoxIntersection(vec3(obj.aabbMinX,obj.aabbMinY,obj.aabbMinZ), vec3(obj.aabbMaxX,obj.aabbMaxY,obj.aabbMaxZ), ray))
    {
        // For each object's triangle
        for(uint i = 0; i < obj.triNumber; i++)
        {
            Triangle tri = triangles[obj.firstTriangle + i];
            float outT, outU, outV;
            if(embreeIntersect(
                tri,
                ray,
                closestHit.rayT, outU, outV))
            //if(rayTriangleIntersect(ray.origin, ray.direction, tri.v0, tri.v0 - vec3(tri.edgeAx,tri.edgeAy,tri.edgeAz), vec3(tri.edgeBx,tri.edgeBy,tri.edgeBz) + tri.v0, outT, outV, outU))
            {
                closestHit.vboStartIndex = obj.vboStartIndex;
                closestHit.attrs = obj.vertexAttrs;
                closestHit.material = obj.material;
                closestHit.totalAttrSize = obj.totalAttrsSize;
                closestHit.barycentric = vec2(outV, outU);
                closestHit.indices = tri.attributeIndices.xyz;
                closestHit.normal = vec3(tri.normalX, tri.normalY, tri.normalZ);
            }
        }
    }
}

bool resolveRay(Ray ray, out vec3 albedo, out vec3 normal, out vec3 emission, out float depth)
{
    //float far = uProj[2].w / (uProj[2].z + 1.0);
    float far = 1000;
    Hit closestHit;
    closestHit.rayT = far;
    for (uint o = 0; o < uObjectCount; o++)
    {
        ObjectDefinition obj = objectDefinitions[o];
        testVisibility(obj, ray, closestHit);
    }
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
            surfaceNormal = interpolate3(closestHit.vboStartIndex, closestHit.barycentric, closestHit.totalAttrSize, currentAttrOffset, closestHit.indices);
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

void updateOutput(vec3 albedo, vec3 normal, float depth, ivec2 coord)
{
    float depthLower = mod(depth,25.5);
    imageStore(uScreenAlbedoDepth, coord, vec4(albedo, (depth/25.5 - depthLower)/25.5));
    imageStore(uScreenNormalDepth, coord, vec4(normal * 0.5 + 0.5, (depthLower)/25.5));
}

vec3 sample_light(vec2 rng)
{
	return lights[0].position + vec3(rng.x - 0.5, 0, rng.y - 0.5) * lights[0].size;
}

vec3 rayTraceSubPixel(vec2 ndcCoord) {
    Ray primaryRay;
    getRay(ndcCoord, primaryRay);
    ivec2 coord = ivec2(gl_FragCoord);
    vec3 albedo, normal, emission;
    float depth;
    if(uRayParameters.x == 0)
    {
        // This is primary ray
        if(resolveRay(primaryRay, albedo, normal, emission, depth))
        {
            updateOutput(albedo, normal, depth, coord);
            return albedo + emission;
        }
        else
        {
            // raytrace-plane
	        float h = (-1-primaryRay.origin.y)/primaryRay.direction.y;
	        if( h>0.0 ) 
	        { 
		        vec3 pos = primaryRay.origin+ h * primaryRay.direction;
		        return vec3(xorTextureGradBox(pos.xz,dFdx(pos.xz),dFdy(pos.xz))*0.5);
	        }
            updateOutput(vec3(0), vec3(0), 0, coord);
            return vec3(0.1);
        }
    }
    else
    {
        // This is a secondary ray
        vec4 albedoDepth = imageLoad(uScreenAlbedoDepth, coord);// Albedo is our throughput now
        vec3 albedo = albedoDepth.rgb;
        vec3 throughput = vec3(1.0);
        vec4 normalDepth = imageLoad(uScreenNormalDepth, coord);
        vec3 previousNormal = normalDepth.xyz * 2. - 1.;
        float depth = albedoDepth.w * 25.5 * 25.5 + normalDepth.w * 25.5;

        vec3 secondaryColor;
        vec3 contrib = vec3(0);
        for(int i = 0; i < NUM_BOUNCES; i++)
        {
            vec3 position = primaryRay.origin + primaryRay.direction * depth;
            { // Next event estimation
			    vec3 pos_ls = sample_light(get_random(gl_FragCoord.xy, position));
                vec3 dirToLight = pos_ls - position;
			    vec3 l_nee = dirToLight;
			    float rr_nee = dot(l_nee, l_nee);
			    l_nee /= sqrt(rr_nee);
			    float G = max(0.0, dot(previousNormal, l_nee)) * max(0.0, -dot(l_nee, lights[0].normal)) / rr_nee;

			    if(G > 0.0) {
				    float light_pdf = 1.0 / (lights[0].area * G);
				    float brdf_pdf = 1.0 / PI;

				    float w = light_pdf / (light_pdf + brdf_pdf);

				    vec3 brdf = albedo.rgb / PI;

                    // Test light visibility
                    float far = length(dirToLight);
                   
                    Ray shadowRay = Ray(position, dirToLight / far);
                    Hit closestHit;
                    closestHit.rayT = far;//constant far plane for lights
                    return vec3(1);
                    for(uint o = 0; o < uObjectCount; o++)
				    {
                        if(o == lights[0].object)
                        {
                            continue;
                        }
                        ObjectDefinition obj = objectDefinitions[o];
                        testVisibility(obj, shadowRay, closestHit);
                    }
                    if(closestHit.rayT == far) {
					    vec3 Le = lights[0].color.xyz;
                        return Le;
					    contrib += throughput * (Le * w * brdf) / light_pdf;
				    }
			    }
		    }
            { /* brdf */
                Ray secondary = createSecondaryRay(ndcCoord, position, previousNormal);
                if(resolveRay(secondary, secondaryColor, normal, emission, depth))
                {
			        vec3 brdf = albedo.rgb / PI;

			        float brdf_pdf = 1.0 / PI;

			        if(emission.x > 0. || emission.y > 0. || emission.z > 0.) { /* hit light_source */
				        float G = max(0.0, dot(secondary.direction, previousNormal)) * max(0.0, -dot(secondary.direction, normal)) / (depth * depth);
				        if(G <= 0.0) /* hit back side of light source */
					        break;

				        float light_pdf = 1.0 / (lights[0].area * G);

				        float w = brdf_pdf / (light_pdf + brdf_pdf);

				        vec3 Le = lights[0].color.rgb;
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

        return contrib;
    }
}

void main() {
    vec3 col = vec3(0);
    /*for(subpI = 0; subpI < 3; subpI++)
    {
        col[subpI] = rayTraceSubPixel(vNDCpos)[subpI];
    }*/
    col = rayTraceSubPixel(vNDCpos);
	
    // output
	OutColor = vec4( col, 1.0 );
}