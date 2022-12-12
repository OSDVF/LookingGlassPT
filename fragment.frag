//!#version 430
#extension GL_ARB_bindless_texture: enable
//#extension GL_ARB_gpu_shader_int64 : enable

#ifdef FLAT_SCREEN
#define getRay getFlatScreenRay
#else
#define getRay getLookingGlassRay
#endif

#ifndef MAX_OBJECT_BUFFER
#define MAX_OBJECT_BUFFER 256
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
// uint material number (if higher than 2 147 483 648, the material is treated as color-only)
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
};

layout(shared, binding = 1) uniform ObjectBuffer {
    ObjectDefinition[MAX_OBJECT_BUFFER] objectDefinitions;
};


uniform uint uObjectCount = 0;
uniform float uTime = 0;
uniform vec2 uWindowSize = vec2(500,500);
uniform vec2 uWindowPos;
uniform vec2 uMouse = vec2(0.5,0.5);
uniform float uViewCone = radians(20); // 40 deg in rad
uniform float uFocusDistance = 10;

layout(std430, binding = 2) readonly buffer AttributeBuffer {
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
    uvec2 samplerOrColorRG;
    float colorB;
};

layout(std430, binding = 3) readonly buffer TriangleBuffer {
    Triangle[] triangles;
};

// The layout is actually dynamically constructed
layout(std430, binding = 4) readonly buffer MaterialBuffer {
    Material[] materials;
};

//TODO vyzkoušet jiné typy projekce, které budou generovat ménì artefaktù

uniform mat4 uView;
uniform mat4 uProj;

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct AnalyticalObject
{
    vec3 position;
    uint type; //0 = sphere, 1 = cube
    vec3 size;
    uint materialIndex;
};

struct Light {
    vec3 direction;
};

AnalyticalObject[1] objects = {
    AnalyticalObject(
        vec3(0,0,-5),
        0,
        vec3(1),
        0
    )
};

Light[1] lights = {
    Light(vec3(0, 1, 0))
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

//https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool solveQuadratic(float a, float b, float c, out float x1, out float x2) 
{ 
    if (b == 0) { 
        // Handle special case where the the two vector ray.dir and V are perpendicular
        // with V = ray.orig - sphere.centre
        if (a == 0) return false; 

        x1 = 0;
        x2 = sqrt(-c / a); 
        return true; 
    } 
    float discr = b * b - 4 * a * c; 
 
    if (discr < 0) return false; 
 
    float q = (b < 0.f) ? -0.5f * (b - sqrt(discr)) : -0.5f * (b + sqrt(discr)); 
    x1 = q / a; 
    x2 = c / q; 
 
    return true; 
} 
//Analytic solution
bool raySphereIntersection(vec3 position, float radius, Ray ray, out float t0, out float t1)
{
    // They ray dir is normalized so A = 1 
    vec3 L = ray.origin - position;
    float A = dot(ray.direction, ray.direction); 
    float B = 2 * dot(ray.direction, L); 
    float C = dot(L, L) - radius * radius; 
 
    if (!solveQuadratic(A, B, C, t0, t1)) return false;  
    if (t0 > t1)
    {
        float swap = t0;
        t0 = t1;
        t1 = swap;
    }
    return true; 
}

bool rayBoxIntersection(vec3 position, vec3 size, Ray ray, out float tmin, out float tmax)
{
    // https://tavianator.com/2011/ray_box.html
    vec3 minPos = position;
    vec3 maxPos = position + size;

    vec3 t1 = (minPos - ray.origin)/ray.direction;
    vec3 t2 = (maxPos - ray.origin)/ray.direction;

    tmin = max(
            min(t1.x, t2.x), 
            max(
                min(t1.y, t2.y), min(t1.z, t2.z)
            )
        );
    tmax = min(
            max(t1.x, t2.x),
            min(
                max(t1.y, t2.y), max(t1.z, t2.z)
            )
        );

    if(tmax < 0 || tmin > tmax)
        return false;

    return true;
}

// With normal
bool getRayBoxIntersection(AnalyticalObject box, Ray ray, out vec3 hitPosition, out vec3 normalAtHit, out float t0)
{
    vec3 minPos = box.position;//Lower left edge
    vec3 maxPos = box.position + box.size;//Upper right edge

    vec3 t1 = (minPos - ray.origin)/ray.direction;
    vec3 t2 = (maxPos - ray.origin)/ray.direction;

    vec3 tmin = 
        vec3(
            min(t1.x, t2.x),
            min(t1.y, t2.y),
            min(t1.z, t2.z)
        );
    vec3 tmax =
        vec3(
            max(t1.x, t2.x),
            max(t1.y, t2.y),
            max(t1.z, t2.z)
        );

    float allMax = min(min(tmax.x,tmax.y),tmax.z);
    float allMin = max(max(tmin.x,tmin.y),tmin.z);

    if(allMax > 0.0 && allMax > allMin)
    {
        hitPosition = ray.origin + ray.direction * allMin;
        vec3 center = box.position + box.size*0.5;
        vec3 difference = center - hitPosition;
        if (allMin == tmin.x) {// Ray hit the X plane
            normalAtHit = vec3(-1, 0, 0) * sign(difference.x);
        }
        else if (allMin == tmin.y) {// Ray hit the Y plane
            normalAtHit = vec3(0, -1, 0) * sign(difference.y);
        }
        else if (allMin == tmin.z) {// Ray hit the Z plane
            normalAtHit = vec3(0, 0, -1) * sign(difference.z);
        } 
        t0 = allMin;
        return true;
    }
    return false;
}


//Geometric solution, with normal
bool getRaySphereIntersection(AnalyticalObject sphere, Ray ray, out vec3 hitPosition, out vec3 normalAtHit, out float t0)
{
    hitPosition = vec3(0,0,0);
    normalAtHit = vec3(0,0,0);
    vec3 L = sphere.position - ray.origin;
    float tca = dot(ray.direction, L);
    if(tca < 0)
        return false;
    float radius2 = sphere.size.x * sphere.size.x;
    float d2 = dot(L,L) - tca * tca; 
    if (d2 > radius2) return false; 

    float thc = sqrt(radius2 - d2); 
    t0 = tca - thc; 
    float t1 = tca + thc; 

    vec3 hp = ray.origin + ray.direction * t0; // point of intersection 
    hitPosition = hp;
    normalAtHit = normalize(hp - sphere.position);
    return true; 
}


bool rayObjectIntersection(AnalyticalObject object, Ray ray, out float t0, out float t1)
{
    switch(object.type)
    {
    case 0:
        //Sphere
        return raySphereIntersection(object.position, object.size.x, ray, t0, t1);
    case 1:
        return rayBoxIntersection(object.position, object.size, ray, t0, t1);
    }
}

bool getRayObjectIntersection(AnalyticalObject object, Ray ray, out vec3 hitPosition, out vec3 normalAtHit, out float t0)
{
    switch(object.type)
    {
    case 0:
        //Sphere
        return getRaySphereIntersection(object, ray, hitPosition, normalAtHit, t0);
    case 1:
        return getRayBoxIntersection(object, ray, hitPosition, normalAtHit, t0);
    }
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
bool embreeIntersect(const Triangle tri, const Ray ray,
    inout float T, out float U, out float V){
    vec3 edgeA = vec3(tri.edgeBx,tri.edgeBy,tri.edgeBz);
    vec3 edgeB = vec3(tri.edgeAx,tri.edgeAy,tri.edgeAz);
    vec3 normal = vec3(tri.normalX,tri.normalY,tri.normalZ);
    #ifdef MAGIC
    const float den = dot( normal, ray.direction );
    if(den < 0.001)
    {
        return false;
    }
    vec3 C = tri.v0 - ray.origin;
    
    vec3 R = cross(ray.direction, C);
    
    const float absDen = abs( den );
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
//#define CULLING
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
 
 float interpolateAttribute(in uint vboStartIndex, in vec2 barycentric, in uint totalAttrSize,
                            in uint currentAttrOffset, in uvec3 indices)
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

vec3 interpolate3(in uint vboStartIndex, in vec2 barycentric, in uint totalAttrSize,
                    in uint currentAttrOffset, uvec3 indices)
{
    vec3 result;
    for(uint attrPart = 0; attrPart < 3; attrPart++)
    {
        result[attrPart] = interpolateAttribute(vboStartIndex, barycentric, totalAttrSize, currentAttrOffset + attrPart, indices);
    }
    return result;
}

vec4 interpolate4(in uint vboStartIndex, in vec2 barycentric, in uint totalAttrSize,
                    in uint currentAttrOffset, uvec3 indices)
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
//#define DEBUG
vec3 getMaterialColor(uint materialIndex, vec2 uv)
{
    vec3 color;
#ifdef DEBUG
    return vec3(uv, 0);
#endif
    Material mat = materials[materialIndex];
    if(mat.isTexture == 1)
    {
        // This is effectively uint64_t
        sampler2D samp = sampler2D(mat.samplerOrColorRG);
        // Return a color from a bindless texture
        uv.y = 1 - uv.y; // UVs are flipped for some reason
        return texture(samp, uv).xyz;
    }
    else
    {
        return vec3(uintBitsToFloat(mat.samplerOrColorRG), mat.colorB);
    }
    return vec3(1);
}

vec3 rayTraceSubPixel(vec2 fragCoord) {
    Ray ray;
    getRay(fragCoord, ray);
    //return ray.direction;
	//getLookignGlassRay( fragCoord + vec2(1.0/3.0,0.0), ddx_ro, ddx_rd );
	//getLookignGlassRay( fragCoord + vec2(0.0,    1.0), ddy_ro, ddy_rd );
    
    vec3 col;
    //float far = uProj[2].w / (uProj[2].z + 1.0);
    float far = 1000;
    Hit closestHit;
    closestHit.rayT = far;
    for (uint o = 0; o < uObjectCount; o++)
    {
        ObjectDefinition obj = objectDefinitions[o];

        // For each triangle
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
    if(closestHit.rayT != far)
    {
        // Interpolate other triangle attributes by barycentric coordinates
        uint currentAttrOffset = 0;
        vec3 surfaceNormal = normalize(closestHit.normal);
        vec3 materialColor = vec3(1., 1., 1.);
        vec4 vertexColor = vec4(1., 1., 1., 0.);
        if ((closestHit.attrs & 1u) != 0)
        {
            //Has vertex colors
            vertexColor = interpolate4(closestHit.vboStartIndex, closestHit.barycentric, closestHit.totalAttrSize, currentAttrOffset, closestHit.indices);
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
            vec2 uv = interpolate2(closestHit.vboStartIndex, closestHit.barycentric, closestHit.totalAttrSize, currentAttrOffset, closestHit.indices);
            return getMaterialColor(closestHit.material, uv);
            currentAttrOffset += 2;
        }
        return vec3(vertexColor);
    }
    vec3 outPos, outNorm;
    float outT;
    if(getRayBoxIntersection(AnalyticalObject(vec3(0,-1,0), 1, vec3(100,0.1,100),0),ray, outPos, outNorm, outT))
    {
        return outNorm * 0.5 + 0.5;
    }
    return vec3(0.1);

    // gamma correction	
	col = pow( col, vec3(0.4545) );

    // line
	return col;// * smoothstep( 1.0, 2.0, abs(fragCoord.x-uWindowSize.x/2.0) );
}

// TODO: subroutine selection

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