//!#version 430
#ifdef FLAT_SCREEN
#define getRay getFlatScreenRay
#else
#define getRay getLookingGlassRay
#endif
out vec4 OutColor;
in vec2 vNDCpos;

layout(shared)
uniform Calibration {
    float pitch;
    float tilt;
    float center;
    float subp;
    vec2 resolution;
} uCalibration;
uniform float uTime = 0;
uniform vec2 uWindowSize = vec2(500,500);
uniform vec2 uWindowPos;
uniform vec2 uMouse = vec2(0.5,0.5);
uniform float uViewCone = radians(20); // 40 deg in rad
uniform float uFocusDistance = 10;

layout(std430, binding=1) buffer VertexBuffer {
    vec3[] vertices;
};

layout(std430, binding=2) buffer IndexBuffer {
    uint[] indices;
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

#define MOLLER_TRUMBORE
#define CULLING
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
    t = -(dot(N, orig) + d) / NdotRayDirection; 
 
    // check if the triangle is in behind the ray
    if (t < 0) return false;  //the triangle is behind 
 
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
 

vec3 rayTraceSubPixel(vec2 fragCoord) {
    Ray ray;
    getRay(fragCoord, ray);
    //return ray.direction;
	//getLookignGlassRay( fragCoord + vec2(1.0/3.0,0.0), ddx_ro, ddx_rd );
	//getLookignGlassRay( fragCoord + vec2(0.0,    1.0), ddy_ro, ddy_rd );
    
    vec3 col;
    for(uint i = 0; i < indices.length(); i+=3)
    {
        vec3[3] triangle;
        for(uint vertInTri = 0; vertInTri < 3; vertInTri++)
        {
            // Pull vertices and cosntruct a triangle
            triangle[vertInTri] = vertices[indices[i + vertInTri]];
        }
        float outT, outU, outV;
        if(rayTriangleIntersect(ray.origin, ray.direction,
            triangle[0],
            triangle[1],
            triangle[2],
            outT, outU, outV))
         {
            return vec3(1);
         }
         else
         {
            vec3 outPos, outNorm;
            if(getRayBoxIntersection(AnalyticalObject(vec3(0,-1,0), 1, vec3(100,0.1,100),0),ray, outPos, outNorm, outT))
            {
                return outNorm * 0.5 + 0.5;
            }
            return vec3(0);
         }
        
    }
    // gamma correction	
	col = pow( col, vec3(0.4545) );

    // line
	return col;// * smoothstep( 1.0, 2.0, abs(fragCoord.x-uWindowSize.x/2.0) );
}

// TODO: subroutine selection

void main() {
    vec3 col = vec3(0);
    for(subpI = 0; subpI < 3; subpI++)
    {
        col[subpI] = rayTraceSubPixel(vNDCpos)[subpI];
    }
	
    // output
	OutColor = vec4( col, 1.0 );
}