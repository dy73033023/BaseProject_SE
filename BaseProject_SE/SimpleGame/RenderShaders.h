#pragma once
// Embedded GLSL keeps all launch paths self-contained. UI bypasses the world post pass.
namespace RenderShaders {
const char* SceneVertex=R"GLSL(#version 330 core
layout(location=0) in vec2 pos;
layout(location=1) in vec4 color;
layout(location=2) in vec2 uv;
out vec4 tint;out vec2 texcoord;
void main(){gl_Position=vec4(pos.x/640.-1.,1.-pos.y/400.,0,1);tint=color;texcoord=uv;}
)GLSL";
const char* SceneFragment=R"GLSL(#version 330 core
in vec4 tint;in vec2 texcoord;out vec4 result;
uniform sampler2D image;uniform int textured;uniform int material;uniform float clock;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1)),f.x),f.y);}
float fbm(vec2 p){float n=0.,a=.5;for(int i=0;i<5;i++){n+=noise(p)*a;p=p*2.03+13.7;a*=.5;}return n;}
float heightAt(vec2 p){return fbm(p*.035)*.7+noise(p*.24)*.2+noise(p*1.2)*.1;}
void main(){
 result=tint;
 if(textured==1){result.a*=texture(image,texcoord).r;return;}
 if(textured==2){result*=texture(image,texcoord);if(result.a<.008)discard;return;}
 if(textured!=3)return;
 vec2 p=texcoord;float n=heightAt(p),fine=noise(p*.6);
 vec3 albedo;float rough=.8,wet=0.;
 if(material==0){
   float moss=smoothstep(.41,.64,fbm(p*.007));wet=1.-smoothstep(.34,.48,fbm(p*.012+75.));
   albedo=mix(vec3(.20,.16,.10),vec3(.15,.23,.095),moss)*(.65+n*.8);
   albedo=mix(albedo,vec3(.08,.15,.145),wet*.7);rough=mix(.85,.18,wet);
 }else if(material==1){
   float track=1.-smoothstep(.05,.10,abs(fract(p.x*.024+p.y*.013)-.5));
   albedo=mix(vec3(.27,.23,.16),vec3(.13,.105,.07),track*.6)*(.65+n*.8);
   wet=smoothstep(.56,.73,fbm(p*.023));rough=.45;
 }else if(material==2){
   vec2 cell=fract(p/85.);float joint=1.-smoothstep(.012,.045,min(min(cell.x,cell.y),min(1.-cell.x,1.-cell.y)));
   float crack=1.-smoothstep(.01,.035,abs(noise(p*.028)-.50));
   albedo=mix(vec3(.36,.39,.34)*(.6+n*.7),vec3(.10,.13,.10),joint*.8+crack*.18);
   albedo=mix(albedo,vec3(.15,.22,.10),smoothstep(.62,.8,fbm(p*.033))*.8);rough=.7;
 }else if(material==3){
   vec2 c=fract(p/62.);float seam=1.-smoothstep(.008,.025,min(min(c.x,c.y),min(1.-c.x,1.-c.y)));
   float scratch=pow(noise(vec2(p.x*.06,p.y*1.7)),14.);
   albedo=vec3(.30,.33,.32)*(.8+n*.4)+scratch*.25;
   albedo=mix(albedo,vec3(.22,.095,.035),smoothstep(.58,.76,fbm(p*.034))*.7);
   albedo*=1.-seam*.65;rough=.27;
 }else if(material==4){
   float ripple=sin(p.x*.08+p.y*.04+clock*1.6)*sin(p.y*.07-clock*1.2);
   albedo=vec3(.08,.19,.19)+ripple*.012;wet=1.;rough=.1;n+=ripple*.03;
 }else if(material==5){
   float grain=fbm(vec2(p.x*.13,p.y*.012));albedo=mix(vec3(.08,.07,.045),vec3(.24,.24,.13),grain);rough=.9;
 }else{albedo=mix(vec3(.045,.11,.045),vec3(.21,.33,.095),n);rough=.7;}
 vec2 grad=vec2(heightAt(p+vec2(1,0))-heightAt(p-vec2(1,0)),heightAt(p+vec2(0,1))-heightAt(p-vec2(0,1)));
 vec3 normal=normalize(vec3(-grad*3.,1.));vec3 light=normalize(vec3(-.6,-.8,1.2));
 float diffuse=max(0.,dot(normal,light));float spec=pow(max(0.,dot(normal,normalize(light+vec3(0,0,1)))),mix(8.,100.,1.-rough));
 albedo*=.57+diffuse*.8;albedo+=vec3(.53,.65,.59)*spec*(.045+wet*.22);
 result=vec4(albedo*tint.rgb,tint.a);
}
)GLSL";
const char* PostVertex=R"GLSL(#version 330 core
out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.-1.,0,1);}
)GLSL";
const char* PostFragment=R"GLSL(#version 330 core
in vec2 uv;out vec4 result;uniform sampler2D source;uniform sampler2D bloom;
uniform int effect;uniform vec2 direction;uniform float clock;uniform float injury;
vec3 film(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
void main(){
 vec3 c=texture(source,uv).rgb;
 if(effect==0){float l=max(c.r,max(c.g,c.b));result=vec4(c*smoothstep(.62,1.3,l),1);return;}
 if(effect==1){
   c*=.227027;c+=texture(source,uv+direction*1.384615).rgb*.316216;
   c+=texture(source,uv-direction*1.384615).rgb*.316216;
   c+=texture(source,uv+direction*3.230769).rgb*.070270;
   c+=texture(source,uv-direction*3.230769).rgb*.070270;result=vec4(c,1);return;
 }
 c+=texture(bloom,uv).rgb*.38;
 // Restrained filmic grade: warm direct light, cool shadows, readable midtones.
 c=film(c*1.32);float l=dot(c,vec3(.2126,.7152,.0722));
 c=mix(vec3(l),c,.91);c*=vec3(1.035,1.015,.965);
 vec2 q=uv-.5;float vignette=1.-smoothstep(.28,.73,length(q))*.26;
 c*=vignette;float grain=fract(sin(dot(gl_FragCoord.xy+floor(clock*12.),vec2(12.9898,78.233)))*43758.5453)-.5;
 c+=grain*.006;
 c=mix(c,vec3(.34,.015,.009),injury*smoothstep(.22,.64,length(q))*.65);
 result=vec4(c,1);
}
)GLSL";
}
