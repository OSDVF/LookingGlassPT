//!#version 420
#ifdef FLAT_SCREEN
#define getRay getFlatScreenRay
#else
#define getRay getLookingGlassRay
#endif
out vec4 OutColor;
in vec2 vNDCpos;

// Based on https://www.shadertoy.com/view/ttXSDN
layout(shared)
uniform Calibration {
    float pitch;
    float tilt;
    float center;
    float subp;//This is not really used
    vec2 resolution;
} uCalibration;
uniform float uTime = 0;
uniform vec2 uWindowSize = vec2(500,500);
uniform vec2 uWindowPos;
uniform vec2 uMouse = vec2(0.5,0.5);

uniform mat4 uView; //
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


float screenSize = 2.0; // Just do everything in screenSizes

// A single view will _converge_ on a point about two screen-heights 
// from the device. Assume focalDist is ~2 screenSizehs backwards, 
// and from ~-0.22222 to 2 screenSizes on the x axis (use mouse.xy 
// to control these)
void getLookingGlassRay(vec2 pix, out Ray ray) {
    // Mouse controlled focal distance and viewpoint spread
    float screenWidth = (uWindowSize.x/uWindowSize.y) * screenSize;
    float focalLeft   = -((uMouse.x/uWindowSize.x)-0.45)*2.0;  
    float focalRight  = screenWidth - focalLeft;
    float focalDist   = -1.0-(uMouse.y/uWindowSize.y) * 5.0; // -2.0 is a good default
    
    // Normalized pixel coordinates (from 0 to 1)
    vec2 screenCoord = pix*0.5+0.5;
    
    // Get the current view for this subpixel
    float view = screenCoord.x;
	view += screenCoord.y * uCalibration.tilt;
	view *= uCalibration.pitch;
	view -= uCalibration.center;
	view = 1.0 - mod(view + ceil(abs(view)), 1.0);
    
    // Calculate the ray dir assuming pixels of a given view converge
    // at points along a line segment floating "focalDist" above the display.
    // TODO: Take into account the refraction of the acrylic, which changes
    // the rays' angle of attack as they converge on pxFoc
    vec3 pxPos = vec3(screenCoord.x * screenWidth, screenCoord.y * screenSize, 0.0);
    vec3 pxFoc = vec3(mix(focalLeft, focalRight, view), 0.5, focalDist);
    vec3 pxDir = pxFoc - pxPos; pxDir /= length(pxDir);
    vec3 pxOri = pxPos + (1.0 * pxDir); // <- Increase for protruding objects
    vec3 rayOrigin  = pxOri; 
    vec3 rayDir = -pxDir;
    ray = Ray(rayOrigin, rayDir);
    ray.origin = vec3(-ray.origin.z, ray.origin.y, ray.origin.x) + vec3(3.2,0.0,-0.9);
}


float Camera_fovY = tan(radians(60));

/*void getFlatScreenRay(vec2 pix, out Ray ray)
{
    float Camera_fovX = (uWindowSize.x * Camera_fovY) / uWindowSize.y;
    // Apply FOV = create perspective projection
    float a = Camera_fovX * pix.x;
    float b = Camera_fovY * pix.y;

    vec3 dir = normalize(a * vec3(1,0,0) + b * vec3(0,1,0) + vec3(0,0,1));

    ray= Ray(vec3(0,0,0), dir);
}*/

/*void getFlatScreenRay(vec2 pix, out Ray ray) {
    vec4 screenSpaceFar = vec4(vNDCpos, 1.0, 0.0);
    vec4 screenSpaceNear = vec4(vNDCpos, 0.0, 0.0);
    vec4 far = inverse(uProj * uView) * screenSpaceFar;
    far /= far.w;
    vec4 near = inverse(uProj * uView) * screenSpaceNear;
    near /= near.w;
    ray = Ray(vec3(0), normalize(far.xyz - near.xyz));
}*/

void getFlatScreenRay(vec2 pix, out Ray ray){
  vec4 dir = inverse(uProj * uView)*vec4(pix,1,1);
  dir.xyz/=dir.w;
  vec3 pos = vec3(inverse(uView)*vec4(0,0,0,1));

  ray = Ray(pos, normalize(dir.xyz));
  ray.origin = vec3(-ray.origin.z, ray.origin.y, ray.origin.x) + vec3(4.2,1,-0.4);
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

// The MIT License
// Copyright © 2019 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


// A filtered xor pattern. Since an xor patterns is a series of checkerboard
// patterns, I derived this one from https://www.shadertoy.com/view/XlcSz2

// Other filterable simple procedural patterns:
//
// checker, 2D, box filter: https://www.shadertoy.com/view/XlcSz2
// checker, 3D, box filter: https://www.shadertoy.com/view/XlXBWs
// checker, 3D, tri filter: https://www.shadertoy.com/view/llffWs
// grid,    2D, box filter: https://www.shadertoy.com/view/XtBfzz
// xor,     2D, box filter: https://www.shadertoy.com/view/tdBXRW
//
// Article: https://iquilezles.org/articles/filterableprocedurals


// --- analytically box-filtered xor pattern ---

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

// --- unfiltered xor pattern ---

float xorTexture( in vec2 pos )
{
    float xor = 0.0;
    for( int i=0; i<8; i++ )
    {
        xor += mod( floor(pos.x)+floor(pos.y), 2.0 );

        pos *= 0.5;
        xor *= 0.5;
    }
    return xor;
}

//===============================================================================================
//===============================================================================================
// sphere implementation
//===============================================================================================
//===============================================================================================

float softShadowSphere( in vec3 ro, in vec3 rd, in vec4 sph )
{
    vec3 oc = sph.xyz - ro;
    float b = dot( oc, rd );
	
    float res = 1.0;
    if( b>0.0 )
    {
        float h = dot(oc,oc) - b*b - sph.w*sph.w;
        res = smoothstep( 0.0, 1.0, 2.0*h/b );
    }
    return res;
}

float occSphere( in vec4 sph, in vec3 pos, in vec3 nor )
{
    vec3 di = sph.xyz - pos;
    float l = length(di);
    return 1.0 - dot(nor,di/l)*sph.w*sph.w/(l*l); 
}

float iSphere( in vec3 ro, in vec3 rd, in vec4 sph )
{
    float t = -1.0;
	vec3  ce = ro - sph.xyz;
	float b = dot( rd, ce );
	float c = dot( ce, ce ) - sph.w*sph.w;
	float h = b*b - c;
	if( h>0.0 )
	{
		t = -b - sqrt(h);
	}
	return t;
}

//===============================================================================================
//===============================================================================================
// scene
//===============================================================================================
//===============================================================================================


// spheres
const vec4 sc0 = vec4(  3.0, 0.5, 0.0, 0.5 );
const vec4 sc1 = vec4( -4.0, 2.0,-5.0, 2.0 );
const vec4 sc2 = vec4( -4.0, 2.0, 5.0, 2.0 );
const vec4 sc3 = vec4(-30.0, 8.0, 0.0, 8.0 );

float intersect( vec3 ro, vec3 rd, out vec3 pos, out vec3 nor, out float occ, out int matid )
{
    // raytrace
	float tmin = 10000.0;
	nor = vec3(0.0);
	occ = 1.0;
	pos = vec3(0.0);
    matid = -1;
	
	// raytrace-plane
	float h = (0.01-ro.y)/rd.y;
	if( h>0.0 ) 
	{ 
		tmin = h; 
		nor = vec3(0.0,1.0,0.0); 
		pos = ro + h*rd;
		matid = 0;
		occ = occSphere( sc0, pos, nor ) * 
			  occSphere( sc1, pos, nor ) *
			  occSphere( sc2, pos, nor ) *
			  occSphere( sc3, pos, nor );
	}


	// raytrace-sphere
	h = iSphere( ro, rd, sc0 );
	if( h>0.0 && h<tmin ) 
	{ 
		tmin = h; 
        pos = ro + h*rd;
		nor = normalize(pos-sc0.xyz); 
		matid = 1;
		occ = 0.5 + 0.5*nor.y;
	}

	h = iSphere( ro, rd, sc1 );
	if( h>0.0 && h<tmin ) 
	{ 
		tmin = h; 
        pos = ro + tmin*rd;
		nor = normalize(pos-sc1.xyz); 
		matid = 2;
		occ = 0.5 + 0.5*nor.y;
	}

	h = iSphere( ro, rd, sc2 );
	if( h>0.0 && h<tmin ) 
	{ 
		tmin = h; 
        pos = ro + tmin*rd;
		nor = normalize(pos-sc2.xyz); 
		matid = 3;
		occ = 0.5 + 0.5*nor.y;
	}

	h = iSphere( ro, rd, sc3 );
	if( h>0.0 && h<tmin ) 
	{ 
		tmin = h; 
        pos = ro + tmin*rd;
		nor = normalize(pos-sc3.xyz); 
		matid = 4;
		occ = 0.5 + 0.5*nor.y;
	}

	return tmin;	
}

vec2 texCoords( in vec3 pos, int mid )
{
    vec2 matuv;
    
    if( mid==0 )
    {
        matuv = pos.xz;
    }
    else if( mid==1 )
    {
        vec3 q = normalize( pos - sc0.xyz );
        matuv = vec2( atan(q.x,q.z), acos(q.y ) )*sc0.w;
    }
    else if( mid==2 )
    {
        vec3 q = normalize( pos - sc1.xyz );
        matuv = vec2( atan(q.x,q.z), acos(q.y ) )*sc1.w;
    }
    else if( mid==3 )
    {
        vec3 q = normalize( pos - sc2.xyz );
        matuv = vec2( atan(q.x,q.z), acos(q.y ) )*sc2.w;
    }
    else if( mid==4 )
    {
        vec3 q = normalize( pos - sc3.xyz );
        matuv = vec2( atan(q.x,q.z), acos(q.y ) )*sc3.w;
    }

	return 200.0*matuv;
}


void calcCamera( out vec3 ro, out vec3 ta )
{
	float an = 0.1*sin(0.1*uTime
   );
	ro = vec3( 5.0*cos(an), 0.5, 5.0*sin(an) );
    ta = vec3( 0.0, 1.0, 0.0 );
}

vec3 doLighting( in vec3 pos, in vec3 nor, in float occ, in vec3 rd )
{
    float sh = min( min( min( softShadowSphere( pos, vec3(0.57703), sc0 ),
				              softShadowSphere( pos, vec3(0.57703), sc1 )),
				              softShadowSphere( pos, vec3(0.57703), sc2 )),
                              softShadowSphere( pos, vec3(0.57703), sc3 ));
	float dif = clamp(dot(nor,vec3(0.57703)),0.0,1.0);
	float bac = clamp(0.5+0.5*dot(nor,vec3(-0.707,0.0,-0.707)),0.0,1.0);
    vec3 lin  = dif*vec3(1.50,1.40,1.30)*sh;
	     lin += occ*vec3(0.15,0.20,0.30);
	     lin += bac*vec3(0.10,0.10,0.10)*(0.2+0.8*occ);

    return lin;
}

//===============================================================================================
//===============================================================================================
// render
//===============================================================================================
//===============================================================================================

vec3 rayTraceSubPixel(vec2 fragCoord) {
    Ray ray;
    getRay(fragCoord, ray);
	//getLookignGlassRay( fragCoord + vec2(1.0/3.0,0.0), ddx_ro, ddx_rd );
	//getLookignGlassRay( fragCoord + vec2(0.0,    1.0), ddy_ro, ddy_rd );
    #if 0
    for(uint i = 0; i < objects.length(); i++)
    {
        vec3 outHit, outNorm;
        float outT;
        bool hit = getRayObjectIntersection(objects[i], ray, outHit, outNorm, outT);
        if(hit)
        {
            return vec3(1)*max(dot(outNorm, lights[0].direction), 0.0);
        }
        else
        {
            return vec3(0);
        }
    }
		#endif
    ray.direction = vec3(-ray.direction.z, ray.direction.y, ray.direction.x);
    // trace
	vec3 pos, nor;
	float occ;
    int mid;
    float t = intersect( ray.origin, ray.direction, pos, nor, occ, mid );

	vec3 col = vec3(0.9);
	if( mid!=-1 )
	{
		// -----------------------------------------------------------------------
        // compute ray differentials by intersecting the tangent plane to the  
        // surface.		
		// -----------------------------------------------------------------------

		// computer ray differentials
		//vec3 ddx_pos = ddx_ro - ddx_rd*dot(ddx_ro-pos,nor)/dot(ddx_rd,nor);
		//vec3 ddy_pos = ddy_ro - ddy_rd*dot(ddy_ro-pos,nor)/dot(ddy_rd,nor);

		// calc texture sampling footprint		
		vec2     uv = texCoords(     pos, mid );
       
		// shading		
		vec3 mate = vec3(0.0);
        #if 0
	    bool lr = fragCoord.x < uWindowSize.x/2.0;
        if( lr ) mate = vec3(1.0)*xorTexture( uv );
        else     mate = vec3(1.0)*xorTextureGradBox( uv, ddx_uv, ddy_uv );
        #else
        //mate = vec3(1.0)*xorTexture( uv );
        //mate = vec3(1.0)*xorTextureGradBox( uv, ddx_uv, ddy_uv );
        mate = vec3(1.0)*xorTextureGradBox( uv, dFdx(uv), dFdy(uv));
        #endif
        mate = pow( mate, vec3(1.5) );
        
        // lighting	
		vec3 lin = doLighting( pos, nor, occ, ray.direction );

        // combine lighting with material		
		col = mate * lin;
		
        // fog		
        col = mix( col, vec3(0.9), 1.0-exp( -0.00001*t*t ) );
	}
	
    // gamma correction	
	col = pow( col, vec3(0.4545) );

    // line
	return col;// * smoothstep( 1.0, 2.0, abs(fragCoord.x-uWindowSize.x/2.0) );
}

// TODO: subroutine selection

void main() {
    vec3 col = vec3(0);
    for(int subpI = 0; subpI < 3; subpI++)
    {
        col[subpI] = rayTraceSubPixel(vNDCpos + vec2(subpI*uCalibration.subp, 0.0))[subpI];
    }
	
    // output
	OutColor = vec4( col, 1.0 );
}