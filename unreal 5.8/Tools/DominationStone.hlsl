// Original, texture-free Domination stone. Broad slabs suit the entity scale;
// restrained joints and slow weather patterns keep targets visually distinct.
struct StoneField {
 float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float noise(float2 p) {
  float2 i=floor(p),f=frac(p); f=f*f*(3-2*f);
  return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
 }
};
StoneField field;
float3 N=normalize(SurfaceN),a=abs(N);
float floorFace=step(.65,a.z);
float2 plane=floorFace>0?WorldP.xy:(a.x>a.y?WorldP.yz:WorldP.xz);
float2 scale=lerp(float2(900,560),float2(1150,900),floorFace);
float2 uv=plane/scale;
uv.x+=floor(uv.y)*.5;
float2 cell=floor(uv),local=frac(uv),edge=min(local,1-local)*scale;
float distance=min(edge.x,edge.y),aa=max(fwidth(distance),.6);
float joint=1-smoothstep(3-aa,9+aa,distance);
float edgeWear=1-smoothstep(8,32,distance);
float grain=field.noise(plane*.018);
float weather=field.noise(plane*.00045+float2(WorldP.z*.00007,7));
float runoff=field.noise(plane*float2(.0018,.00018));
float block=field.hash(cell);
float masonry=saturate(Masonry),trim=saturate(SurfaceStyle),metal=saturate(SurfaceStyle-1);
float damp=(1-smoothstep(-1200,1800,WorldP.z))*(.4+weather*.6);
float stoneTone=.88+block*.16+(weather-.5)*.16+(grain-.5)*.045;
stoneTone-=damp*.085+(1-floorFace)*runoff*.06;
stoneTone=lerp(stoneTone,stoneTone*.48,joint*masonry);
stoneTone+=edgeWear*.035*masonry;
float cutTone=.91+(weather-.5)*.08+(grain-.5)*.035;
float rockTone=.82+weather*.20+(grain-.5)*.06;
float tone=lerp(lerp(rockTone,stoneTone,masonry),cutTone,trim);
float luma=dot(Tint.rgb,float3(.2126,.7152,.0722));
float3 color=lerp(luma.xxx,Tint.rgb,.65);
// North slate and south iron have comparable value, with different subtle hue.
float3 region=WorldP.y>=0?float3(.94,1.025,1.045):float3(1.045,.99,1.015);
color*=lerp(region,1.xxx,trim);
StoneRoughness=lerp(clamp(.86+(weather-.5)*.08+joint*.04,.79,.96),.62,trim);
StoneRoughness=lerp(StoneRoughness,.44+(weather-.5)*.08,metal);
StoneMetallic=metal*.65;
float height=(grain-.5)*.55-joint*1.3*masonry*(1-trim);
float3 dx=ddx(WorldP),dy=ddy(WorldP),r1=cross(dy,N),r2=cross(N,dx);
float det=dot(dx,r1);
float3 grad=sign(det)*(ddx(height)*r1+ddy(height)*r2);
StoneNormal=normalize(max(abs(det),.0001)*N-grad);
return max(color*tone,float3(.008,.009,.01));
